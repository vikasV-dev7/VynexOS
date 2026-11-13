#include "desktop_shell.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/hal/input_driver.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace vynexos::apps {

constexpr int kDragThreshold = 3;

DesktopShell::DesktopShell(std::shared_ptr<desktop::IWindowManager> wm,
                           std::shared_ptr<desktop::ICompositor> compositor,
                           std::shared_ptr<desktop::IWidgetToolkit> toolkit,
                           std::shared_ptr<desktop::IWindowDecorator> decorator,
                           std::shared_ptr<desktop::ITaskbar> taskbar,
                           std::shared_ptr<core::IEventBus> event_bus)
    : m_wm(std::move(wm)), m_compositor(std::move(compositor)), 
      m_toolkit(std::move(toolkit)), m_decorator(std::move(decorator)), 
      m_taskbar(std::move(taskbar)), m_event_bus(std::move(event_bus)) {
      
    m_display_size = m_compositor->logical_size();

    m_theme = desktop::to_window_theme(m_toolkit->theme());
      
    m_subscriptions.push_back(m_event_bus->subscribe("notification_show", [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(desktop::Notification)) {
            std::lock_guard lock(m_mutex);
            auto n = std::any_cast<desktop::Notification>(ev->payload);
            m_notifications.push_back(n);
        }
    }));

    m_subscriptions.push_back(m_event_bus->subscribe("notification_dismiss", [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(uint32_t)) {
            std::lock_guard lock(m_mutex);
            auto id = std::any_cast<uint32_t>(ev->payload);
            std::erase_if(m_notifications, [id](const auto& n) { return n.id == id; });
        }
    }));

    m_subscriptions.push_back(m_event_bus->subscribe(hal::kTopicInputMouse, [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(hal::MouseEvent)) {
            auto mouse_ev = std::any_cast<hal::MouseEvent>(ev->payload);
            
            m_compositor->invalidate_region({m_cursor_x, m_cursor_y, 16, 16});
            m_cursor_x = mouse_ev.x;
            m_cursor_y = mouse_ev.y;
            m_compositor->invalidate_region({m_cursor_x, m_cursor_y, 16, 16});

            if (m_input_suppressed) {
                // After a cancelled interaction, buttons still held are ignored until everything has been released.
                if (mouse_ev.button_state != 0) return;
                m_input_suppressed = false;
                m_last_mouse_state = 0;
            }
            validate_captures();

            bool clicked = (mouse_ev.button_state != 0 && m_last_mouse_state == 0);
            bool released = (mouse_ev.button_state == 0 && m_last_mouse_state != 0);
            bool dragging = (mouse_ev.button_state != 0 && m_last_mouse_state != 0);
            
            m_last_mouse_state = mouse_ev.button_state;
            
            if (clicked) { // Click / Drag Candidate Creation
                set_hover({}); // hover feedback is for a free pointer; a press takes over

                // Vynex launcher button, anchored to the taskbar at the bottom of the logical desktop.
                const int32_t button_top = display_height() - kTaskbarHeight + kLauncherButtonY;
                if (mouse_ev.x >= kLauncherButtonX && mouse_ev.x <= kLauncherButtonX + kLauncherButtonWidth &&
                    mouse_ev.y >= button_top && mouse_ev.y <= button_top + kLauncherButtonHeight) {
                    bool show = false;
                    {
                        std::lock_guard lock(m_mutex);
                        show = !m_launcher_visible;
                    }
                    set_launcher_visible(show);
                    return; // Consumed by DesktopShell
                }

                // Focus Management, Resize & Drag Start
                auto target_window = m_wm->get_window_at(mouse_ev.x, mouse_ev.y);
                auto resize_handle = m_wm->get_resize_handle_at(mouse_ev.x, mouse_ev.y);

                bool launcher_open = false;
                {
                    std::lock_guard lock(m_mutex);
                    launcher_open = m_launcher_visible;
                }
                if (launcher_open) {
                    if (target_window == std::optional<uint32_t>{m_launcher_id}) {
                        // A press on the launcher only selects the entry under it: it acts on release, and it neither
                        // focuses the launcher nor reaches anything beneath.
                        const auto entry = launcher_entry_at(mouse_ev.x, mouse_ev.y);
                        std::lock_guard lock(m_mutex);
                        m_launcher_press = entry;
                        m_drag_state.window_id = std::nullopt;
                        m_resize_state.window_id = std::nullopt;
                        return;
                    }
                    set_launcher_visible(false); // a press anywhere else dismisses it, then acts as usual
                }

                if (target_window.has_value() && target_window.value() == m_taskbar_id) {
                    auto geom = m_wm->get_window_geometry(m_taskbar_id);
                    if (geom.has_value()) {
                        int32_t rel_x = mouse_ev.x - geom->x;
                        int32_t rel_y = mouse_ev.y - geom->y;
                        auto clicked_win = m_taskbar->get_clicked_window(rel_x, rel_y);
                        if (clicked_win.has_value()) {
                            uint32_t wid = clicked_win.value();
                            
                            // Taskbar button: restore and focus a minimized window, minimize the focused one, focus any other.
                            if (m_wm->get_window_state(wid) == desktop::WindowState::Minimized) {
                                // Restore to previous state (Normal or Maximized)
                                invalidate_window_region(wid);
                                m_wm->set_window_state(wid, m_wm->get_previous_window_state(wid));
                                invalidate_window_region(wid);
                                this->focus_window(wid);
                            } else if (focused_window() == std::optional<uint32_t>{wid}) {
                                invalidate_window_region(wid);
                                m_wm->set_window_state(wid, desktop::WindowState::Minimized);
                                invalidate_window_region(wid);
                                focus_next_after(wid);
                            } else {
                                this->focus_window(wid);
                            }
                            return; // Consumed
                        }
                    }
                }
                
                if (resize_handle.has_value() && !is_shell_window(resize_handle->window_id)) {
                    if (m_wm->get_window_state(resize_handle->window_id) == desktop::WindowState::Maximized) {
                        return; // Resizing disabled for Maximized windows
                    }
                    this->focus_window(resize_handle->window_id);
                    auto geom = m_wm->get_window_geometry(resize_handle->window_id);
                    if (geom.has_value()) {
                        std::lock_guard lock(m_mutex);
                        m_resize_state.window_id = resize_handle->window_id;
                        m_resize_state.edge = resize_handle->edge;
                        m_resize_state.initial_mouse_x = mouse_ev.x;
                        m_resize_state.initial_mouse_y = mouse_ev.y;
                        m_resize_state.initial_geometry = *geom;
                        m_resize_state.pointer_captured = false;
                        
                        m_drag_state.window_id = std::nullopt; // Ensure mutually exclusive
                    }
                    return;
                }
                
                if (target_window.has_value() && target_window.value() != m_wallpaper_id && target_window.value() != m_taskbar_id) {
                    auto dec_hit = m_wm->get_decoration_hit_region(target_window.value(), mouse_ev.x, mouse_ev.y);

                    if (dec_hit == desktop::DecorationHitRegion::CloseButton || dec_hit == desktop::DecorationHitRegion::MinimizeButton ||
                        dec_hit == desktop::DecorationHitRegion::MaximizeButton) {
                        // Controls act on release, not on press: the press only captures the pointer.
                        begin_control_press({target_window.value(), dec_hit});
                        return;
                    }
                    
                    if (dec_hit == desktop::DecorationHitRegion::TitleBar) {
                        if (m_wm->get_window_state(target_window.value()) == desktop::WindowState::Maximized) {
                            // Focus but do not start drag if Maximized
                            this->focus_window(target_window.value());
                            m_wm->focus_window(target_window.value());
                            return;
                        }
                        this->focus_window(target_window.value());
                        auto geom = m_wm->get_window_geometry(target_window.value());
                        if (geom.has_value()) {
                            std::lock_guard lock(m_mutex);
                            m_drag_state.window_id = target_window.value();
                            m_drag_state.initial_mouse_x = mouse_ev.x;
                            m_drag_state.initial_mouse_y = mouse_ev.y;
                            m_drag_state.initial_window_x = geom->x;
                            m_drag_state.initial_window_y = geom->y;
                            m_drag_state.pointer_captured = false;
                            
                            m_resize_state.window_id = std::nullopt;
                        }
                        return;
                    }
                }
                
                if (target_window.has_value()) {
                    // Focus policy for presses: an application window takes the focus; the desktop takes it away (no
                    // window has the keyboard); the taskbar never takes it (only its buttons act, above).
                    if (!is_shell_window(target_window.value())) {
                        this->focus_window(target_window.value());
                    } else if (target_window.value() == m_wallpaper_id) {
                        this->clear_focus();
                    }
                    // We no longer initiate window drag from clicking the content area
                    std::lock_guard lock(m_mutex);
                    m_drag_state.window_id = std::nullopt;
                    m_resize_state.window_id = std::nullopt;
                } else {
                    this->clear_focus();
                    std::lock_guard lock(m_mutex);
                    m_drag_state.window_id = std::nullopt;
                    m_resize_state.window_id = std::nullopt;
                }
            } else if (dragging) { // Drag Update
                bool pressing_control = false;
                {
                    std::lock_guard lock(m_mutex);
                    pressing_control = m_pointer.press.window_id.has_value();
                    if (m_launcher_press.has_value()) return; // a launcher press: nothing happens until the release
                }
                if (pressing_control) {
                    track_control_press(mouse_ev.x, mouse_ev.y); // captured: nothing else sees this pointer
                    return;
                }
                std::unique_lock lock(m_mutex);
                if (m_resize_state.window_id.has_value()) {
                    if (!m_resize_state.pointer_captured) {
                        if (std::abs(mouse_ev.x - m_resize_state.initial_mouse_x) > kDragThreshold ||
                            std::abs(mouse_ev.y - m_resize_state.initial_mouse_y) > kDragThreshold) {
                            m_resize_state.pointer_captured = true;
                        }
                    }
                    
                    if (m_resize_state.pointer_captured) {
                        int32_t delta_x = mouse_ev.x - m_resize_state.initial_mouse_x;
                        int32_t delta_y = mouse_ev.y - m_resize_state.initial_mouse_y;
                        
                        const desktop::WindowGeometry initial = m_resize_state.initial_geometry;
                        desktop::WindowGeometry geom = initial;
                        auto edge = m_resize_state.edge;
                        const bool left = edge == desktop::ResizeEdge::Left || edge == desktop::ResizeEdge::TopLeft || edge == desktop::ResizeEdge::BottomLeft;
                        const bool top = edge == desktop::ResizeEdge::Top || edge == desktop::ResizeEdge::TopLeft || edge == desktop::ResizeEdge::TopRight;
                        
                        if (left) {
                            geom.x += delta_x;
                            geom.width = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.width) - delta_x));
                        } else if (edge == desktop::ResizeEdge::Right || edge == desktop::ResizeEdge::TopRight || edge == desktop::ResizeEdge::BottomRight) {
                            geom.width = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.width) + delta_x));
                        }
                        
                        if (top) {
                            geom.y += delta_y;
                            geom.height = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.height) - delta_y));
                        } else if (edge == desktop::ResizeEdge::Bottom || edge == desktop::ResizeEdge::BottomLeft || edge == desktop::ResizeEdge::BottomRight) {
                            geom.height = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.height) + delta_y));
                        }
                        
                        uint32_t resize_wid = m_resize_state.window_id.value();
                        if (top) {
                            // The top edge stops where the title bar would leave the work area (placement policy); the
                            // bottom edge stays put.
                            const auto placed = desktop::constrain_window_position(geom, m_wm->get_window_margins(resize_wid), m_wm->get_work_area());
                            if (placed.y > geom.y) {
                                const int64_t bottom = static_cast<int64_t>(geom.y) + geom.height;
                                geom.height = static_cast<uint32_t>(std::max<int64_t>(0, bottom - placed.y));
                                geom.y = placed.y;
                            }
                        }
                        invalidate_window_region(resize_wid);
                        // A size the window manager rejects (beyond the surface limits) leaves the window as it was.
                        const bool resized = m_wm->set_window_geometry(resize_wid, geom);
                        // The window manager enforces a minimum size. Resizing from the left or top edge must not then
                        // push the window along: the opposite edge stays where it was.
                        if (auto applied = m_wm->get_window_geometry(resize_wid); resized && applied && (left || top) &&
                                                                                   (applied->width != geom.width || applied->height != geom.height)) {
                            desktop::WindowGeometry anchored = *applied;
                            if (left) anchored.x = initial.x + static_cast<int32_t>(initial.width) - static_cast<int32_t>(applied->width);
                            if (top) anchored.y = initial.y + static_cast<int32_t>(initial.height) - static_cast<int32_t>(applied->height);
                            m_wm->set_window_geometry(resize_wid, anchored);
                        }
                        invalidate_window_region(resize_wid);
                        
                        // Redraw decorations since dimensions changed
                        // Unlock to call update_window_decoration which locks inside
                        uint32_t wid = m_resize_state.window_id.value();
                        bool is_focused = (m_focused_window.has_value() && m_focused_window.value() == wid);
                        lock.unlock();
                        this->update_window_decoration(wid, is_focused);
                        lock.lock();
                    }
                } else if (m_drag_state.window_id.has_value()) {
                    if (!m_drag_state.pointer_captured) {
                        // Check Drag Threshold
                        if (std::abs(mouse_ev.x - m_drag_state.initial_mouse_x) > kDragThreshold ||
                            std::abs(mouse_ev.y - m_drag_state.initial_mouse_y) > kDragThreshold) {
                            m_drag_state.pointer_captured = true;
                        }
                    }
                    
                    if (m_drag_state.pointer_captured) {
                        // Calculate Drag Delta
                        int32_t delta_x = mouse_ev.x - m_drag_state.initial_mouse_x;
                        int32_t delta_y = mouse_ev.y - m_drag_state.initial_mouse_y;
                        
                        int32_t new_x = m_drag_state.initial_window_x + delta_x;
                        int32_t new_y = m_drag_state.initial_window_y + delta_y;
                        
                        // Dispatch to IWindowManager
                        uint32_t drag_wid = m_drag_state.window_id.value();
                        invalidate_window_region(drag_wid);
                        place_window(drag_wid, new_x, new_y);
                        invalidate_window_region(drag_wid);
                    }
                }
            } else if (released) { // Drag Completion / Release Pointer Capture
                bool pressing_control = false;
                std::optional<size_t> launcher_press;
                {
                    std::lock_guard lock(m_mutex);
                    pressing_control = m_pointer.press.window_id.has_value();
                    launcher_press = std::exchange(m_launcher_press, std::nullopt);
                    m_drag_state.window_id = std::nullopt;
                    m_drag_state.pointer_captured = false;
                    m_resize_state.window_id = std::nullopt;
                    m_resize_state.pointer_captured = false;
                }
                if (launcher_press.has_value()) {
                    // An entry acts only when released over the entry that was pressed.
                    if (launcher_entry_at(mouse_ev.x, mouse_ev.y) == launcher_press && m_host) {
                        const auto entries = m_host->launcher_entries();
                        set_launcher_visible(false);
                        if (*launcher_press < entries.size()) m_host->open(entries[*launcher_press].app_id);
                    }
                    return;
                }
                if (pressing_control) {
                    release_control_press(mouse_ev.x, mouse_ev.y);
                    return;
                }
                set_hover(control_at(mouse_ev.x, mouse_ev.y));
            } else if (mouse_ev.button_state == 0) { // Free pointer motion
                set_hover(control_at(mouse_ev.x, mouse_ev.y));
            }
            
            route_pointer_to_application(mouse_ev, clicked, released);
        }
    }));
    
    m_subscriptions.push_back(m_event_bus->subscribe(hal::kTopicInputPointer, [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() != typeid(hal::PointerSignalEvent)) return;
        const auto signal = std::any_cast<hal::PointerSignalEvent>(ev->payload).signal;
        if (signal == hal::PointerSignal::Cancel) {
            cancel_interaction();
        } else {
            set_hover({}); // the pointer is gone; a press in progress keeps its capture
        }
    }));

    m_subscriptions.push_back(m_event_bus->subscribe(hal::kTopicInputKey, [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(hal::KeyEvent)) route_key(*ev, std::any_cast<hal::KeyEvent>(ev->payload));
    }));

    m_subscriptions.push_back(m_event_bus->subscribe(hal::kTopicInputText, [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(hal::TextInputEvent)) route_text(*ev);
    }));
}

DesktopShell::~DesktopShell() {
    for (const auto id : m_subscriptions) m_event_bus->unsubscribe(id);
}

void DesktopShell::launch() {
    m_wm->set_work_area({0, 0, m_display_size.width, m_display_size.height - kTaskbarHeight});
    
    // 1. Wallpaper (Fullscreen)
    desktop::WindowGeometry wall_geom{0, 0, m_display_size.width, m_display_size.height};
    m_wallpaper_id = m_wm->create_window("Wallpaper", wall_geom);
    
    // The shell's own surfaces follow the display size; one the limits refuse is simply absent (nothing drawn there).
    m_wallpaper_surface = m_compositor->create_surface(wall_geom.width, wall_geom.height);
    if (m_wallpaper_surface) {
        auto map_wall = m_wallpaper_surface->map_pixels();
        for (size_t i = 0; i < map_wall.size(); i += 4) {
            map_wall[i] = static_cast<uint8_t>(30);
            map_wall[i+1] = static_cast<uint8_t>(30);
            map_wall[i+2] = static_cast<uint8_t>(30);
            map_wall[i+3] = static_cast<uint8_t>(255);
        }
        m_wallpaper_surface->unmap_pixels();
    }
    m_wm->set_window_surface(m_wallpaper_id, m_wallpaper_surface);
    
    // 2. Taskbar (Bottom panel)
    desktop::WindowGeometry task_geom{0, display_height() - kTaskbarHeight, m_display_size.width, kTaskbarHeight};
    m_taskbar_id = m_wm->create_window("Taskbar", task_geom);
    
    m_taskbar_surface = m_compositor->create_surface(task_geom.width, task_geom.height);
    if (m_taskbar_surface) {
        auto map_task = m_taskbar_surface->map_pixels();
        std::fill(map_task.begin(), map_task.end(), static_cast<uint8_t>(0));
        m_taskbar_surface->unmap_pixels();
    }
    m_wm->set_window_surface(m_taskbar_id, m_taskbar_surface);
    
    // 3. Launcher Window
    desktop::WindowGeometry launcher_geom{10, display_height() - kTaskbarHeight - 400, 300, 400};
    m_launcher_id = m_wm->create_window("Launcher", launcher_geom);
    
    m_launcher_surface = m_compositor->create_surface(launcher_geom.width, launcher_geom.height);
    m_wm->set_window_surface(m_launcher_id, m_launcher_surface);
    m_wm->set_window_visibility(m_launcher_id, false);

    m_wm->set_window_layer(m_wallpaper_id, desktop::WindowLayer::Desktop);
    m_wm->set_window_layer(m_taskbar_id, desktop::WindowLayer::Panel);
    m_wm->set_window_layer(m_launcher_id, desktop::WindowLayer::Overlay);
}

void DesktopShell::update_frame() {
    if (m_wallpaper_id == 0 || m_taskbar_id == 0) return;
    
    // Synchronize Taskbar state
    bool taskbar_changed = false;
    {
        std::vector<desktop::TaskbarEntry> entries;
        const auto active_win = focused_window(); // the shell's focus is the one the taskbar shows
        auto snapshots = m_wm->get_window_snapshots();
        
        std::vector<uint32_t> live_windows;
        live_windows.reserve(snapshots.size());
        for (const auto& snap : snapshots) {
            if (is_shell_window(snap.window_id)) continue; // no frame, no taskbar entry

            live_windows.push_back(snap.window_id);
            sync_window_chrome(snap);

            desktop::TaskbarEntry entry;
            entry.window_id = snap.window_id;
            entry.title = snap.title;
            entry.state = snap.state;
            entry.is_active = (active_win.has_value() && active_win.value() == snap.window_id);
            entry.attention_requested = false;
            entries.push_back(std::move(entry));
        }
        taskbar_changed = m_taskbar->synchronize(entries);

        std::lock_guard lock(m_mutex);
        std::erase_if(m_chrome, [&](const auto& item) { return std::find(live_windows.begin(), live_windows.end(), item.first) == live_windows.end(); });
    }
    
    if ((taskbar_changed || !m_first_frame_drawn) && m_taskbar_surface) {
        m_first_frame_drawn = true;
        // Clear Taskbar surface
        auto map_task = m_taskbar_surface->map_pixels();
        std::fill(map_task.begin(), map_task.end(), static_cast<uint8_t>(0));
        m_taskbar_surface->unmap_pixels();
        
        // Draw Taskbar using toolkit (native ISurface interface)
        m_toolkit->draw_panel(m_taskbar_surface, 0, 0, m_display_size.width, kTaskbarHeight);
        m_toolkit->draw_button(m_taskbar_surface, kLauncherButtonX, kLauncherButtonY, kLauncherButtonWidth, kLauncherButtonHeight, "Vynex");
        m_taskbar->draw(m_taskbar_surface);
        m_toolkit->draw_text(m_taskbar_surface, display_width() - 70, 15, "12:00", desktop::Color{255, 255, 255, 255});
        
        invalidate_window_region(m_taskbar_id);
    }
    
    sync_notifications();

    {
        std::lock_guard lock(m_mutex);
        // Draw Launcher
        if (m_launcher_visible && m_launcher_surface) {
            // Clear Launcher surface
            {
                auto map_launcher = m_launcher_surface->map_pixels();
                std::fill(map_launcher.begin(), map_launcher.end(), static_cast<uint8_t>(0));
                m_launcher_surface->unmap_pixels();
            }
            
            m_toolkit->draw_panel(m_launcher_surface, 0, 0, 300, 400);
            m_toolkit->draw_rect(m_launcher_surface, 0, 0, 300, 30, desktop::Color{50, 50, 70, 255});
            m_toolkit->draw_text(m_launcher_surface, 10, 10, "Applications", desktop::Color{255, 255, 255, 255});

            // One button per application the host offers
            if (m_host) {
                const auto entries = m_host->launcher_entries();
                for (size_t i = 0; i < entries.size(); ++i) {
                    const desktop::Rect r = launcher_entry_rect(i);
                    m_toolkit->draw_button(m_launcher_surface, r.x, r.y, r.width, r.height, entries[i].title);
                }
            }
        }
    }
    
    // Update running applications and bring what they changed to the screen
    {
        std::lock_guard lock(m_mutex);
        for (auto& running_app : m_running_apps) {
            if (running_app.app) {
                running_app.app->update(16.6); // Transitional constant delta time
                if (running_app.visible && running_app.surface) {
                    present_application_content(running_app);
                }
            }
        }
    }
}

void DesktopShell::route_pointer_to_application(const hal::MouseEvent& mouse, bool pressed, bool released) {
    // An application sees the pointer only where it is its own: over its client area, or anywhere while a press that
    // began there is held (the press captures the pointer until the release). A press that began anywhere else (title
    // bar, border, controls, the desktop, another window) is never delivered, not even when it crosses the content.
    std::lock_guard lock(m_mutex);
    std::optional<uint32_t> target = m_app_pointer_owner;
    if (!target) {
        if ((mouse.button_state != 0 && !pressed) || released) return; // a press held, or ending, that began elsewhere
        target = m_wm->get_window_at(mouse.x, mouse.y);
        if (!target) return;
    }
    const auto client = m_wm->get_window_geometry(*target);
    const auto app = std::find_if(m_running_apps.begin(), m_running_apps.end(), [&](const auto& a) { return a.window_id == *target; });
    if (!client || app == m_running_apps.end() || !app->visible || !app->app) {
        m_app_pointer_owner.reset(); // its window or application went away during the press
        return;
    }
    if (!m_app_pointer_owner) {
        if (!desktop::client_contains(*client, mouse.x, mouse.y)) return; // over the window's decorations
        if (const auto grip = m_wm->get_resize_handle_at(mouse.x, mouse.y); grip && grip->window_id == *target) return; // a corner grip over the content
    }
    m_app_pointer_owner = (mouse.button_state != 0) ? target : std::nullopt;
deliver_pointer_locked(*target, mouse);
}

bool DesktopShell::deliver_pointer_locked(uint32_t window_id, const hal::MouseEvent& mouse) {
    const auto client = m_wm->get_window_geometry(window_id);
    const auto app = std::find_if(m_running_apps.begin(), m_running_apps.end(), [&](const auto& a) { return a.window_id == window_id; });
    if (!client || app == m_running_apps.end() || !app->visible || !app->app) return false;
    hal::MouseEvent local = mouse; // desktop -> content coordinates
    local.x -= client->x;
    local.y -= client->y;
    core::Event local_ev;
    local_ev.topic = hal::kTopicInputMouse;
    local_ev.payload = local;
    app->app->handle_event(local_ev);
    return true;
}

desktop::RunningApplication* DesktopShell::keyboard_target_locked() {
    if (m_launcher_visible || !m_focused_window.has_value()) return nullptr; // the launcher or the desktop has the keyboard
    const auto app = std::find_if(m_running_apps.begin(), m_running_apps.end(), [&](const auto& a) { return a.window_id == *m_focused_window; });
    if (app == m_running_apps.end() || !app->visible || !app->app) return nullptr;
    if (m_wm->get_window_state(app->window_id) == desktop::WindowState::Minimized) return nullptr;
    return &*app;
}

void DesktopShell::route_key(const core::Event& ev, const hal::KeyEvent& key) {
    bool dismiss_launcher = false;
    {
        std::lock_guard lock(m_mutex);
        if (m_launcher_visible) {
            m_key_presses.erase(key.key_code);
            dismiss_launcher = key.pressed && key.key_code == hal::kKeyEscape;
        } else if (key.pressed) {
            auto* target = keyboard_target_locked();
            if (!target) {
                m_key_presses.erase(key.key_code);
                return;
            }
            m_key_presses[key.key_code] = target->window_id;
            target->app->handle_event(ev);
        } else {
            const auto owner = m_key_presses.find(key.key_code);
            if (owner == m_key_presses.end()) return; // its press reached no application
            const uint32_t window = owner->second;
            m_key_presses.erase(owner);
            auto* target = keyboard_target_locked();
            if (target && target->window_id == window) target->app->handle_event(ev); // only while it still has focus
        }
    }
    if (dismiss_launcher) set_launcher_visible(false);
}

void DesktopShell::route_text(const core::Event& ev) {
    std::lock_guard lock(m_mutex);
    if (auto* target = keyboard_target_locked()) target->app->handle_event(ev);
}

void DesktopShell::set_launcher_visible(bool visible) {
    {
        std::lock_guard lock(m_mutex);
        if (m_launcher_visible == visible) return;
        m_launcher_visible = visible;
        m_launcher_press.reset();
    }
    // The launcher is in the Overlay layer, so it is above every application window and the taskbar without being
    // raised or focused; showing it changes neither the stacking of windows nor the focus.
    m_wm->set_window_visibility(m_launcher_id, visible);
    invalidate_window_region(m_launcher_id); // repaint: the compositor only redraws dirty regions
}

desktop::Rect DesktopShell::notification_rect(size_t index) const {
    // Stacked upwards from just above the taskbar at the right edge, 60 px apart (the layout the wallpaper used to carry).
    const int32_t bottom = display_height() - kTaskbarHeight - 5 - static_cast<int32_t>(index) * kNotificationStep;
    return {display_width() - 320, bottom - kNotificationStep, kNotificationWidth, kNotificationHeight};
}

void DesktopShell::sync_notifications() {
    // Each notification is an Overlay window exactly the size of its panel: above application windows and the taskbar,
    // receiving the pointer over its own pixels only (not in the gaps between panels), never framed or focused. Rebuilt
    // here, on the frame thread, whenever the list changes; the event handlers only edit the list.
    std::vector<desktop::Notification> current;
    {
        std::lock_guard lock(m_mutex);
        current = m_notifications;
    }
    const bool unchanged = current.size() == m_notification_views.size() &&
                           std::equal(current.begin(), current.end(), m_notification_views.begin(),
                                      [](const desktop::Notification& n, const NotificationView& v) { return n.id == v.id; });
    if (unchanged) return;

    for (const NotificationView& view : m_notification_views) {
        if (view.window == 0) continue;
        invalidate_window_region(view.window);
        m_wm->destroy_window(view.window);
    }
    m_notification_views.clear();
    for (size_t i = 0; i < current.size(); ++i) {
        const desktop::Rect r = notification_rect(i);
        auto surface = m_compositor->create_surface(r.width, r.height);
        if (!surface) {
            m_notification_views.push_back({current[i].id, 0}); // refused by the surface limits: listed, not shown
            continue;
        }
        m_toolkit->draw_panel(surface, 0, 0, r.width, r.height);
        m_toolkit->draw_text(surface, 10, 10, current[i].title, desktop::Color{255, 255, 255, 255});
        m_toolkit->draw_text(surface, 10, 30, current[i].body, desktop::Color{200, 200, 200, 255});
        const uint32_t window = m_wm->create_window("Notification", {r.x, r.y, r.width, r.height});
        m_wm->set_window_surface(window, std::move(surface));
        m_wm->set_window_layer(window, desktop::WindowLayer::Overlay);
        m_notification_views.push_back({current[i].id, window});
        invalidate_window_region(window);
    }
}

void DesktopShell::place_window(uint32_t window_id, int32_t x, int32_t y) {
    auto geometry = m_wm->get_window_geometry(window_id);
    if (!geometry) return;
    geometry->x = x;
    geometry->y = y;
    const auto placed = desktop::constrain_window_position(*geometry, m_wm->get_window_margins(window_id), m_wm->get_work_area());
    m_wm->move_window(window_id, placed.x, placed.y);
}

std::optional<uint32_t> DesktopShell::next_focus_candidate(uint32_t leaving) {
    // The topmost visible, not minimized application window other than `leaving` (stacking order, Normal layer only:
    // the wallpaper, the taskbar and the launcher never take the focus).
    const auto stack = m_wm->get_visible_windows_z_ordered();
    std::lock_guard lock(m_mutex);
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if (*it == leaving || is_shell_window(*it) || m_wm->get_window_layer(*it) != desktop::WindowLayer::Normal) continue;
        const bool hosts_application = std::any_of(m_running_apps.begin(), m_running_apps.end(), [&](const auto& a) { return a.window_id == *it && a.app; });
        if (hosts_application) return *it;
    }
    return std::nullopt;
}

void DesktopShell::focus_next_after(uint32_t leaving) {
    if (const auto next = next_focus_candidate(leaving)) {
        focus_window(*next);
    } else {
        clear_focus();
    }
}

std::optional<size_t> DesktopShell::launcher_entry_at(int32_t x, int32_t y) const {
    const auto geom = m_wm->get_window_geometry(m_launcher_id);
    if (!geom || !m_host) return std::nullopt;
    const size_t count = m_host->launcher_entries().size();
    const int32_t lx = x - geom->x, ly = y - geom->y;
    for (size_t i = 0; i < count; ++i) {
        const desktop::Rect r = launcher_entry_rect(i);
        if (lx >= r.x && ly >= r.y && lx < r.x + static_cast<int32_t>(r.width) && ly < r.y + static_cast<int32_t>(r.height)) return i;
    }
    return std::nullopt;
}

void DesktopShell::fit_application_surface(desktop::RunningApplication& app, const desktop::WindowGeometry& client) {
    // An application's surface is exactly its client area. Only a new size needs new pixels; the new surface has not been
    // painted, so the content stamp no longer matches and the whole content is rendered and redrawn.
    if (app.surface->width() == client.width && app.surface->height() == client.height) return;
    auto surface = m_compositor->create_surface(client.width, client.height);
    if (!surface) return;
    m_wm->set_window_surface(app.window_id, surface);
    app.surface = std::move(surface);
}

void DesktopShell::present_application_content(desktop::RunningApplication& app) {
    constexpr size_t kMaxReports = 64;
    const auto client = m_wm->get_window_geometry(app.window_id);
    if (client && (client->width == 0 || client->height == 0)) {
        // No content can be shown. What it reports is dropped; it is painted in full once it has an area again.
        for (size_t taken = 0; taken < kMaxReports && app.app->take_invalidated_region(); ++taken) {}
        m_content.erase(app.window_id);
        return;
    }
    if (client) fit_application_surface(app, *client);

    const desktop::ISurface& surface = *app.surface;
    const auto stamp = m_content.find(app.window_id);
    const bool unpainted = stamp == m_content.end() || stamp->second.surface != &surface || stamp->second.width != surface.width() ||
                           stamp->second.height != surface.height();

    // Drain what the application reports (bounded, so a misbehaving application cannot stall the frame; anything beyond
    // the bound is treated as "everything changed").
    constexpr size_t kMaxRegions = 8;
    std::array<desktop::Rect, kMaxRegions> regions{};
    size_t count = 0;
    bool everything = unpainted;
    for (size_t taken = 0; taken < kMaxReports; ++taken) {
        const auto region = app.app->take_invalidated_region();
        if (!region) break;
        if (count < kMaxRegions) regions[count++] = *region;
        else everything = true;
    }
    if (!everything && count == 0) return; // nothing changed: no rendering, no redraw

    app.app->render(*app.surface);
    m_content[app.window_id] = ContentStamp{&surface, surface.width(), surface.height()};

    if (!client) return;
    if (everything) {
        invalidate_content_region(*client, surface, kWholeContent);
        return;
    }
    for (size_t i = 0; i < count; ++i) invalidate_content_region(*client, surface, regions[i]);
}

void DesktopShell::invalidate_content_region(const desktop::WindowGeometry& client, const desktop::ISurface& surface, const desktop::Rect& region) {
    // Content coordinates -> the part inside the content (the client area, as far as the surface covers it) -> screen.
    const int64_t content_w = std::min<int64_t>(client.width, surface.width());
    const int64_t content_h = std::min<int64_t>(client.height, surface.height());
    const int64_t x0 = std::max<int64_t>(region.x, 0);
    const int64_t y0 = std::max<int64_t>(region.y, 0);
    const int64_t x1 = std::min<int64_t>(static_cast<int64_t>(region.x) + region.width, content_w);
    const int64_t y1 = std::min<int64_t>(static_cast<int64_t>(region.y) + region.height, content_h);
    if (x0 >= x1 || y0 >= y1) return;
    const int64_t sx = client.x + x0, sy = client.y + y0;
    if (sx > std::numeric_limits<int32_t>::max() || sy > std::numeric_limits<int32_t>::max()) return; // far off-screen
    m_compositor->invalidate_region({static_cast<int32_t>(sx), static_cast<int32_t>(sy), static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)});
}

void DesktopShell::add_running_application(desktop::RunningApplication&& app) {
    uint32_t window_id = app.window_id;
    {
        std::lock_guard lock(m_mutex);
        m_running_apps.push_back(std::move(app));
    }
    
    // Generate initial decoration surface
    this->update_window_decoration(window_id, false);
}

void DesktopShell::remove_running_application(uint32_t window_id) {
    bool had_focus = false;
    {
        std::lock_guard lock(m_mutex);
        had_focus = m_focused_window == window_id;
        if (had_focus) m_focused_window = std::nullopt; // never a focus on something that is going away
        for (auto& app : m_running_apps) {
            if (app.window_id == window_id) app.focused = false;
        }
        std::erase_if(m_running_apps, [window_id](const auto& app) { return app.window_id == window_id; });
        m_content.erase(window_id);
        if (m_app_pointer_owner == window_id) m_app_pointer_owner.reset();
    }
    if (had_focus) focus_next_after(window_id);
}

void DesktopShell::clear_running_applications() {
    std::lock_guard lock(m_mutex);
    m_focused_window = std::nullopt; // safely clear focus
    m_running_apps.clear();
    m_content.clear();
    m_app_pointer_owner.reset();
}

size_t DesktopShell::running_application_count() const {
    // We cannot lock a std::mutex in a const method without making the mutex mutable.
    // For now, since size() is a simple read and this is largely for introspection/tests,
    // we return the size directly. We could use a mutable mutex or atomic size if thread contention is expected.
    return m_running_apps.size();
}

void DesktopShell::focus_window(uint32_t window_id) {
    std::optional<uint32_t> old_focus;
    {
        std::lock_guard lock(m_mutex);
        old_focus = m_focused_window;
        m_focused_window = window_id;
        for (auto& app : m_running_apps) {
            app.focused = (app.window_id == window_id);
        }
    }
    m_wm->focus_window(window_id);
    
    // Update decorations for focus changes
    if (old_focus.has_value() && old_focus.value() != window_id) {
        this->update_window_decoration(old_focus.value(), false);
    }
    this->update_window_decoration(window_id, true);
}

void DesktopShell::clear_focus() {
    std::optional<uint32_t> old_focus;
    {
        std::lock_guard lock(m_mutex);
        old_focus = m_focused_window;
        m_focused_window = std::nullopt;
        for (auto& app : m_running_apps) {
            app.focused = false;
        }
    }
    
    if (old_focus.has_value()) {
        this->update_window_decoration(old_focus.value(), false);
    }
}

std::optional<uint32_t> DesktopShell::focused_window() const {
    // Note: m_mutex cannot be locked here as it's not mutable and the method is const.
    // Assuming single-threaded access or safe reading.
    return m_focused_window;
}

desktop::RunningApplication* DesktopShell::focused_application() {
    std::lock_guard lock(m_mutex);
    if (!m_focused_window.has_value()) {
        return nullptr;
    }
    for (auto& app : m_running_apps) {
        if (app.window_id == m_focused_window.value()) {
            return &app;
        }
    }
    return nullptr;
}

bool DesktopShell::has_focus() const {
    return m_focused_window.has_value();
}

namespace {

// The rectangle (decoration-surface coordinates) a control occupies; empty for anything that is not a control.
desktop::Rect control_rect(const desktop::DecorationLayout& layout, desktop::DecorationHitRegion control) {
    switch (control) {
    case desktop::DecorationHitRegion::CloseButton: return layout.close_button;
    case desktop::DecorationHitRegion::MinimizeButton: return layout.minimize_button;
    case desktop::DecorationHitRegion::MaximizeButton: return layout.maximize_button;
    default: return {0, 0, 0, 0};
    }
}

} // namespace

void DesktopShell::update_window_decoration(uint32_t window_id, bool focused) {
    // The decorator owns the frame geometry. Applying it first matters: the decorated bounds depend on the margins.
    m_wm->set_window_frame_styles(window_id, m_decorator->frame_style(desktop::WindowState::Normal),
                                  m_decorator->frame_style(desktop::WindowState::Maximized));

    const auto bounds = m_wm->get_decorated_window_bounds(window_id);
    if (bounds.width == 0 || bounds.height == 0) return;

    // The title always comes from the window manager; the fallback only covers a window that has none.
    std::string title = m_wm->get_window_title(window_id);
    if (title.empty()) title = "Untitled";
    const auto state = m_wm->get_window_state(window_id);

    desktop::Rect client = bounds;
    if (auto client_geom = m_wm->get_window_geometry(window_id)) {
        client = {client_geom->x, client_geom->y, client_geom->width, client_geom->height};
    }

    // Hit-testing in the window manager uses this same style, so what is drawn is what is clickable.
    const desktop::FrameStyle style = m_wm->get_window_frame_style(window_id);
    const desktop::DecorationLayout layout = desktop::calculate_decoration_layout(desktop::make_frame_metrics(style, bounds, client));

    // Held while drawing: chrome is updated from both the input path and the frame loop.
    std::lock_guard lock(m_mutex);
    const desktop::DecorationVisualState visual = chrome_visual_locked(window_id, focused, state);
    ChromeStamp& stamp = m_chrome[window_id];

    const bool same_frame = stamp.surface && stamp.width == bounds.width && stamp.height == bounds.height &&
                            stamp.focused == focused && stamp.state == state && stamp.title == title;
    if (same_frame) {
        if (stamp.hovered == visual.hovered && stamp.pressed == visual.pressed) return; // already showing exactly this

        // Only the interaction feedback changed: repaint the controls it affects, in place, and nothing else.
        std::array<desktop::Rect, 4> dirty{};
        size_t dirty_count = 0;
        for (const auto control : {stamp.hovered, stamp.pressed, visual.hovered, visual.pressed}) {
            const desktop::Rect r = control_rect(layout, control);
            if (r.width == 0 || r.height == 0) continue;
            const bool seen = std::any_of(dirty.begin(), dirty.begin() + static_cast<std::ptrdiff_t>(dirty_count), [&](const desktop::Rect& d) {
                return d.x == r.x && d.y == r.y && d.width == r.width && d.height == r.height;
            });
            if (!seen) dirty[dirty_count++] = r;
        }
        bool repainted = true;
        for (size_t i = 0; i < dirty_count && repainted; ++i) {
            repainted = m_decorator->redraw_decoration_region(stamp.surface, layout, title, visual, m_theme, dirty[i]);
        }
        if (repainted) {
            stamp.hovered = visual.hovered;
            stamp.pressed = visual.pressed;
            for (size_t i = 0; i < dirty_count; ++i) {
                m_compositor->invalidate_region({bounds.x + dirty[i].x, bounds.y + dirty[i].y, dirty[i].width, dirty[i].height});
            }
            return;
        }
        // The decorator could not update just those regions: fall through to a full redraw of the same surface.
    }

    std::shared_ptr<desktop::ISurface> surface = stamp.surface;
    if (!surface || surface->width() != bounds.width || surface->height() != bounds.height) {
        surface = m_compositor->create_surface(bounds.width, bounds.height); // only a new size needs new pixels
        if (!surface) return; // a size the limits refuse: the window keeps the frame it has (retried while it differs)
    }
    m_decorator->draw_decorations(surface, layout, title, visual, m_theme);
    m_wm->set_window_decoration(window_id, surface, style.margins);

    stamp = ChromeStamp{focused, state, bounds.width, bounds.height, std::move(title), visual.hovered, visual.pressed, std::move(surface)};
    invalidate_window_region(window_id);
}

void DesktopShell::sync_window_chrome(const desktop::WindowSnapshot& snapshot) {
    if (snapshot.state == desktop::WindowState::Minimized) return; // nothing is shown; redrawn when restored

    bool focused = false;
    bool up_to_date = false;
    {
        std::lock_guard lock(m_mutex);
        focused = m_focused_window.has_value() && m_focused_window.value() == snapshot.window_id;
        if (const auto it = m_chrome.find(snapshot.window_id); it != m_chrome.end()) {
            const auto& stamp = it->second;
            const auto bounds = m_wm->get_decorated_window_bounds(snapshot.window_id);
            const std::string_view title = snapshot.title.empty() ? std::string_view{"Untitled"} : std::string_view{snapshot.title};
            const auto visual = chrome_visual_locked(snapshot.window_id, focused, snapshot.state);
            up_to_date = stamp.surface && stamp.focused == focused && stamp.state == snapshot.state && stamp.width == bounds.width &&
                         stamp.height == bounds.height && stamp.title == title && stamp.hovered == visual.hovered &&
                         stamp.pressed == visual.pressed;
        }
    }
    if (!up_to_date) update_window_decoration(snapshot.window_id, focused);
}

void DesktopShell::refresh_chrome(uint32_t window_id) {
    bool focused = false;
    {
        std::lock_guard lock(m_mutex);
        focused = m_focused_window.has_value() && m_focused_window.value() == window_id;
    }
    update_window_decoration(window_id, focused);
}

desktop::DecorationVisualState DesktopShell::chrome_visual_locked(uint32_t window_id, bool focused, desktop::WindowState state) const {
    desktop::DecorationVisualState visual;
    visual.focused = focused;
    visual.state = state;
    if (m_pointer.press.window_id.has_value()) {
        // While a control holds the pointer, it is the only one with feedback, and only while the pointer is over it.
        if (m_pointer.press.window_id == window_id && m_pointer.press_inside) {
            visual.hovered = m_pointer.press.control;
            visual.pressed = m_pointer.press.control;
        }
    } else if (m_pointer.hover.window_id == window_id) {
        visual.hovered = m_pointer.hover.control;
    }
    return visual;
}

DesktopShell::ControlTarget DesktopShell::control_at(int32_t x, int32_t y) const {
    const auto window = m_wm->get_window_at(x, y);
    if (!window.has_value() || is_shell_window(window.value())) return {};
    if (m_wm->get_window_state(window.value()) == desktop::WindowState::Minimized) return {};
    if (m_wm->get_resize_handle_at(x, y).has_value()) return {}; // a press here would resize, so it is not a control
    const auto region = m_wm->get_decoration_hit_region(window.value(), x, y);
    if (region == desktop::DecorationHitRegion::CloseButton || region == desktop::DecorationHitRegion::MinimizeButton ||
        region == desktop::DecorationHitRegion::MaximizeButton) {
        return {window, region};
    }
    return {};
}

void DesktopShell::set_hover(const ControlTarget& target) {
    ControlTarget previous;
    {
        std::lock_guard lock(m_mutex);
        if (m_pointer.hover == target) return; // ordinary pointer motion: nothing to repaint
        previous = m_pointer.hover;
        m_pointer.hover = target;
    }
    if (previous.window_id.has_value() && previous.window_id != target.window_id) refresh_chrome(previous.window_id.value());
    if (target.window_id.has_value()) refresh_chrome(target.window_id.value());
}

void DesktopShell::begin_control_press(const ControlTarget& target) {
    if (!target.window_id.has_value()) return;
    const auto state = m_wm->get_window_state(target.window_id.value());
    {
        std::lock_guard lock(m_mutex);
        m_pointer.press = target;
        m_pointer.press_inside = true;
        m_pointer.press_state = state;
        m_drag_state.window_id = std::nullopt;
        m_resize_state.window_id = std::nullopt;
    }
    refresh_chrome(target.window_id.value());
}

void DesktopShell::track_control_press(int32_t x, int32_t y) {
    ControlTarget press;
    {
        std::lock_guard lock(m_mutex);
        press = m_pointer.press;
    }
    if (!press.window_id.has_value()) return;
    const bool inside = (control_at(x, y) == press);
    {
        std::lock_guard lock(m_mutex);
        if (m_pointer.press_inside == inside) return;
        m_pointer.press_inside = inside;
    }
    refresh_chrome(press.window_id.value());
}

void DesktopShell::release_control_press(int32_t x, int32_t y) {
    ControlTarget press;
    {
        std::lock_guard lock(m_mutex);
        press = m_pointer.press;
        m_pointer.press = {};
        m_pointer.press_inside = false;
    }
    if (!press.window_id.has_value()) return;
    // Activation needs the release over the very control that was pressed.
    if (control_at(x, y) == press) {
        activate_control(press.window_id.value(), press.control);
    }
    if (m_wm->get_window_geometry(press.window_id.value()).has_value()) refresh_chrome(press.window_id.value());
    set_hover(control_at(x, y));
}

void DesktopShell::activate_control(uint32_t wid, desktop::DecorationHitRegion control) {
    if (control == desktop::DecorationHitRegion::CloseButton) {
        invalidate_window_region(wid);
        if (m_host && m_host->close(wid)) return; // window, application instance and surface go together
        this->remove_running_application(wid);
        m_wm->destroy_window(wid);
        return;
    }

    if (control == desktop::DecorationHitRegion::MinimizeButton) {
        invalidate_window_region(wid);
        m_wm->set_window_state(wid, desktop::WindowState::Minimized);
        invalidate_window_region(wid);
        if (focused_window() == std::optional<uint32_t>{wid}) focus_next_after(wid);
        return;
    }

    if (control == desktop::DecorationHitRegion::MaximizeButton) {
        auto state = m_wm->get_window_state(wid);
        invalidate_window_region(wid);
        if (state == desktop::WindowState::Normal) {
            m_wm->set_window_state(wid, desktop::WindowState::Maximized);
        } else if (state == desktop::WindowState::Maximized) {
            m_wm->set_window_state(wid, desktop::WindowState::Normal);
        }
        invalidate_window_region(wid);
        this->focus_window(wid);
        m_wm->focus_window(wid);
    }
}

void DesktopShell::validate_captures() {
    // A captured window can disappear or change state underneath the pointer (closed by its application, restored from
    // elsewhere): the interaction it was part of is then over, without activating anything.
    std::optional<uint32_t> press_window;
    desktop::WindowState press_state{};
    std::optional<uint32_t> drag_window;
    std::optional<uint32_t> resize_window;
    {
        std::lock_guard lock(m_mutex);
        press_window = m_pointer.press.window_id;
        press_state = m_pointer.press_state;
        drag_window = m_drag_state.window_id;
        resize_window = m_resize_state.window_id;
    }
    if (!press_window && !drag_window && !resize_window) return; // the common case: nothing captured
    auto lost = [this](const std::optional<uint32_t>& id, desktop::WindowState expected) {
        return id.has_value() && (!m_wm->get_window_geometry(id.value()).has_value() || m_wm->get_window_state(id.value()) != expected);
    };
    const bool press_lost = lost(press_window, press_state);
    const bool drag_lost = lost(drag_window, desktop::WindowState::Normal);
    const bool resize_lost = lost(resize_window, desktop::WindowState::Normal);
    if (!press_lost && !drag_lost && !resize_lost) return;
    {
        std::lock_guard lock(m_mutex);
        if (press_lost) {
            m_pointer.press = {};
            m_pointer.press_inside = false;
        }
        if (drag_lost) m_drag_state = DragState{};
        if (resize_lost) m_resize_state = ResizeState{};
    }
    if (press_lost && m_wm->get_window_geometry(press_window.value()).has_value()) refresh_chrome(press_window.value());
}

void DesktopShell::cancel_interaction() {
    // Interrupted input (e.g. the host window lost focus, so the release may never arrive): abandon the press without
    // activating, end a drag or resize where it is, and drop hover feedback.
    ControlTarget press;
    ControlTarget hover;
    {
        std::lock_guard lock(m_mutex);
        press = m_pointer.press;
        hover = m_pointer.hover;
        m_pointer = ChromePointer{};
        m_drag_state = DragState{};
        m_resize_state = ResizeState{};
        m_launcher_press.reset();
        // An application holding the pointer sees its press end, so it never waits for a release that will not come.
        if (m_app_pointer_owner.has_value()) deliver_pointer_locked(*m_app_pointer_owner, hal::MouseEvent(m_cursor_x, m_cursor_y, 0));
        m_app_pointer_owner.reset();
    }
    m_last_mouse_state = 0;
    m_input_suppressed = true;
    if (press.window_id.has_value()) refresh_chrome(press.window_id.value());
    if (hover.window_id.has_value() && hover.window_id != press.window_id) refresh_chrome(hover.window_id.value());
}

void DesktopShell::invalidate_window_region(uint32_t window_id) {
    auto bounds = m_wm->get_decorated_window_bounds(window_id);
    if (bounds.width > 0 && bounds.height > 0) {
        m_compositor->invalidate_region({
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height
        });
    }
}

} // namespace vynexos::apps
