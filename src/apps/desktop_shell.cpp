#include "desktop_shell.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/hal/input_driver.hpp"

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

    m_theme.active_title_color = {50, 100, 200, 255};
    m_theme.inactive_title_color = {100, 100, 100, 255};
    m_theme.border_color = {80, 80, 80, 255};
    m_theme.background_color = {255, 255, 255, 255};
    m_theme.text_color = {255, 255, 255, 255};
      
    m_event_bus->subscribe("notification_show", [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(desktop::Notification)) {
            std::lock_guard lock(m_mutex);
            auto n = std::any_cast<desktop::Notification>(ev->payload);
            m_notifications.push_back(n);
        }
    });

    m_event_bus->subscribe("notification_dismiss", [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(uint32_t)) {
            std::lock_guard lock(m_mutex);
            auto id = std::any_cast<uint32_t>(ev->payload);
            std::erase_if(m_notifications, [id](const auto& n) { return n.id == id; });
        }
    });

    m_event_bus->subscribe(hal::kTopicInputMouse, [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(hal::MouseEvent)) {
            auto mouse_ev = std::any_cast<hal::MouseEvent>(ev->payload);
            
            m_compositor->invalidate_region({m_cursor_x, m_cursor_y, 16, 16});
            m_cursor_x = mouse_ev.x;
            m_cursor_y = mouse_ev.y;
            m_compositor->invalidate_region({m_cursor_x, m_cursor_y, 16, 16});
            
            bool clicked = (mouse_ev.button_state != 0 && m_last_mouse_state == 0);
            bool released = (mouse_ev.button_state == 0 && m_last_mouse_state != 0);
            bool dragging = (mouse_ev.button_state != 0 && m_last_mouse_state != 0);
            
            m_last_mouse_state = mouse_ev.button_state;
            
            if (clicked) { // Click / Drag Candidate Creation
                // Vynex launcher button, anchored to the taskbar at the bottom of the logical desktop.
                const int32_t button_top = display_height() - kTaskbarHeight + kLauncherButtonY;
                if (mouse_ev.x >= kLauncherButtonX && mouse_ev.x <= kLauncherButtonX + kLauncherButtonWidth &&
                    mouse_ev.y >= button_top && mouse_ev.y <= button_top + kLauncherButtonHeight) {
                    std::lock_guard lock(m_mutex);
                    m_launcher_visible = !m_launcher_visible;
                    m_wm->set_window_visibility(m_launcher_id, m_launcher_visible);
                    invalidate_window_region(m_launcher_id); // repaint: the compositor only redraws dirty regions
                    return; // Consumed by DesktopShell
                }
                
                // Focus Management, Resize & Drag Start
                auto target_window = m_wm->get_window_at(mouse_ev.x, mouse_ev.y);
                auto resize_handle = m_wm->get_resize_handle_at(mouse_ev.x, mouse_ev.y);
                
                if (target_window.has_value() && target_window.value() == m_taskbar_id) {
                    auto geom = m_wm->get_window_geometry(m_taskbar_id);
                    if (geom.has_value()) {
                        int32_t rel_x = mouse_ev.x - geom->x;
                        int32_t rel_y = mouse_ev.y - geom->y;
                        auto clicked_win = m_taskbar->get_clicked_window(rel_x, rel_y);
                        if (clicked_win.has_value()) {
                            uint32_t wid = clicked_win.value();
                            
                            // Taskbar button click logic
                            auto current_state = m_wm->get_window_state(wid);
                            if (current_state == desktop::WindowState::Minimized) {
                                // Restore to previous state (Normal or Maximized)
                                invalidate_window_region(wid);
                                m_wm->set_window_state(wid, m_wm->get_previous_window_state(wid));
                                m_wm->focus_window(wid);
                                invalidate_window_region(wid);
                                this->focus_window(wid);
                            } else {
                                // Window is not minimized
                                std::unique_lock lock(m_mutex);
                                if (m_focused_window.has_value() && m_focused_window.value() == wid) {
                                    // If already focused, minimize it
                                    lock.unlock();
                                    invalidate_window_region(wid);
                                    m_wm->set_window_state(wid, desktop::WindowState::Minimized);
                                    invalidate_window_region(wid);
                                    lock.lock();
                                    
                                    // Focus the next window in z-order
                                    auto visible_windows = m_wm->get_visible_windows_z_ordered();
                                    if (!visible_windows.empty()) {
                                        m_focused_window = visible_windows.back();
                                        m_wm->focus_window(m_focused_window.value());
                                    } else {
                                        m_focused_window = std::nullopt;
                                    }
                                } else {
                                    // Otherwise just focus it
                                    m_wm->focus_window(wid);
                                    m_focused_window = wid;
                                }
                            }
                            return; // Consumed
                        }
                    }
                }
                
                if (resize_handle.has_value() && resize_handle->window_id != m_wallpaper_id && resize_handle->window_id != m_taskbar_id) {
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
                    
                    if (dec_hit == desktop::DecorationHitRegion::CloseButton) {
                        uint32_t wid = target_window.value();
                        invalidate_window_region(wid);
                        this->remove_running_application(wid);
                        m_wm->destroy_window(wid);
                        return;
                    }
                    
                    if (dec_hit == desktop::DecorationHitRegion::MinimizeButton) {
                        uint32_t wid = target_window.value();
                        invalidate_window_region(wid);
                        m_wm->set_window_state(wid, desktop::WindowState::Minimized);
                        invalidate_window_region(wid);
                        
                        // Transfer focus if the minimized window was focused
                        std::lock_guard lock(m_mutex);
                        if (m_focused_window.has_value() && m_focused_window.value() == wid) {
                            auto visible_windows = m_wm->get_visible_windows_z_ordered();
                            if (!visible_windows.empty()) {
                                m_focused_window = visible_windows.back();
                                m_wm->focus_window(m_focused_window.value());
                            } else {
                                m_focused_window = std::nullopt;
                            }
                        }
                        return;
                    }
                    
                    if (dec_hit == desktop::DecorationHitRegion::MaximizeButton) {
                        uint32_t wid = target_window.value();
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
                    if (target_window.value() != m_wallpaper_id && target_window.value() != m_taskbar_id) {
                        this->focus_window(target_window.value());
                        
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
                } else {
                    this->clear_focus();
                    std::lock_guard lock(m_mutex);
                    m_drag_state.window_id = std::nullopt;
                    m_resize_state.window_id = std::nullopt;
                }
            } else if (dragging) { // Drag Update
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
                        
                        desktop::WindowGeometry geom = m_resize_state.initial_geometry;
                        auto edge = m_resize_state.edge;
                        
                        if (edge == desktop::ResizeEdge::Left || edge == desktop::ResizeEdge::TopLeft || edge == desktop::ResizeEdge::BottomLeft) {
                            geom.x += delta_x;
                            geom.width = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.width) - delta_x));
                        } else if (edge == desktop::ResizeEdge::Right || edge == desktop::ResizeEdge::TopRight || edge == desktop::ResizeEdge::BottomRight) {
                            geom.width = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.width) + delta_x));
                        }
                        
                        if (edge == desktop::ResizeEdge::Top || edge == desktop::ResizeEdge::TopLeft || edge == desktop::ResizeEdge::TopRight) {
                            geom.y += delta_y;
                            geom.height = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.height) - delta_y));
                        } else if (edge == desktop::ResizeEdge::Bottom || edge == desktop::ResizeEdge::BottomLeft || edge == desktop::ResizeEdge::BottomRight) {
                            geom.height = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(geom.height) + delta_y));
                        }
                        
                        uint32_t resize_wid = m_resize_state.window_id.value();
                        invalidate_window_region(resize_wid);
                        m_wm->set_window_geometry(resize_wid, geom);
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
                        m_wm->move_window(drag_wid, new_x, new_y);
                        invalidate_window_region(drag_wid);
                    }
                }
            } else if (released) { // Drag Completion / Release Pointer Capture
                std::lock_guard lock(m_mutex);
                m_drag_state.window_id = std::nullopt;
                m_drag_state.pointer_captured = false;
                m_resize_state.window_id = std::nullopt;
                m_resize_state.pointer_captured = false;
            }
            
            // Route mouse event exclusively to the window under cursor OR captured window
            std::optional<uint32_t> target_window_routing;
            {
                std::lock_guard lock(m_mutex);
                if (m_resize_state.pointer_captured && m_resize_state.window_id.has_value()) {
                    target_window_routing = m_resize_state.window_id;
                } else if (m_drag_state.pointer_captured && m_drag_state.window_id.has_value()) {
                    target_window_routing = m_drag_state.window_id; // Pointer Capture
                }
            }
            
            if (!target_window_routing.has_value()) {
                target_window_routing = m_wm->get_window_at(mouse_ev.x, mouse_ev.y);
            }
            
            if (target_window_routing.has_value()) {
                std::lock_guard lock(m_mutex);
                for (auto& running_app : m_running_apps) {
                    if (running_app.window_id == target_window_routing.value() && running_app.visible && running_app.app) {
                        auto geom_opt = m_wm->get_window_geometry(running_app.window_id);
                        if (geom_opt.has_value()) {
                            auto margins = m_wm->get_window_margins(running_app.window_id);
                            
                            // Translate to local coordinates
                            hal::MouseEvent local_mouse_ev = mouse_ev;
                            local_mouse_ev.x -= (geom_opt->x + margins.left);
                            local_mouse_ev.y -= (geom_opt->y + margins.top);
                            
                            auto local_ev = std::make_shared<core::Event>();
                            local_ev->topic = ev->topic;
                            local_ev->payload = local_mouse_ev;
                            
                            running_app.app->handle_event(*local_ev);
                        } else {
                            running_app.app->handle_event(*ev);
                        }
                        break;
                    }
                }
            }
        }
    });
    
    m_event_bus->subscribe(hal::kTopicInputKey, [this](std::shared_ptr<const core::Event> ev) {
        std::lock_guard lock(m_mutex);
        if (m_focused_window.has_value()) {
            for (auto& running_app : m_running_apps) {
                if (running_app.window_id == m_focused_window.value() && running_app.visible && running_app.app) {
                    running_app.app->handle_event(*ev);
                    break;
                }
            }
        }
    });
}

void DesktopShell::launch() {
    m_wm->set_work_area({0, 0, m_display_size.width, m_display_size.height - kTaskbarHeight});
    
    // 1. Wallpaper (Fullscreen)
    desktop::WindowGeometry wall_geom{0, 0, m_display_size.width, m_display_size.height};
    m_wallpaper_id = m_wm->create_window("Wallpaper", wall_geom);
    
    m_wallpaper_surface = m_compositor->create_surface(wall_geom.width, wall_geom.height);
    auto map_wall = m_wallpaper_surface->map_pixels();
    for (size_t i = 0; i < map_wall.size(); i += 4) {
        map_wall[i] = static_cast<uint8_t>(30);
        map_wall[i+1] = static_cast<uint8_t>(30);
        map_wall[i+2] = static_cast<uint8_t>(30);
        map_wall[i+3] = static_cast<uint8_t>(255);
    }
    m_wallpaper_surface->unmap_pixels();
    m_wm->set_window_surface(m_wallpaper_id, m_wallpaper_surface);
    
    // 2. Taskbar (Bottom panel)
    desktop::WindowGeometry task_geom{0, display_height() - kTaskbarHeight, m_display_size.width, kTaskbarHeight};
    m_taskbar_id = m_wm->create_window("Taskbar", task_geom);
    
    m_taskbar_surface = m_compositor->create_surface(task_geom.width, task_geom.height);
    auto map_task = m_taskbar_surface->map_pixels();
    std::fill(map_task.begin(), map_task.end(), static_cast<uint8_t>(0));
    m_taskbar_surface->unmap_pixels();
    m_wm->set_window_surface(m_taskbar_id, m_taskbar_surface);
    
    // 3. Launcher Window
    desktop::WindowGeometry launcher_geom{10, display_height() - kTaskbarHeight - 400, 300, 400};
    m_launcher_id = m_wm->create_window("Launcher", launcher_geom);
    
    m_launcher_surface = m_compositor->create_surface(launcher_geom.width, launcher_geom.height);
    m_wm->set_window_surface(m_launcher_id, m_launcher_surface);
    m_wm->set_window_visibility(m_launcher_id, false);
}

void DesktopShell::update_frame() {
    if (m_wallpaper_id == 0 || m_taskbar_id == 0) return;
    
    // Synchronize Taskbar state
    bool taskbar_changed = false;
    {
        std::vector<desktop::TaskbarEntry> entries;
        auto active_win = m_wm->get_active_window();
        auto snapshots = m_wm->get_window_snapshots();
        
        for (const auto& snap : snapshots) {
            if (snap.window_id == m_wallpaper_id || snap.window_id == m_taskbar_id || snap.window_id == m_launcher_id) {
                continue;
            }
            
            desktop::TaskbarEntry entry;
            entry.window_id = snap.window_id;
            entry.title = snap.title;
            entry.state = snap.state;
            entry.is_active = (active_win.has_value() && active_win.value() == snap.window_id);
            entry.attention_requested = false;
            entries.push_back(std::move(entry));
        }
        taskbar_changed = m_taskbar->synchronize(entries);
    }
    
    if (taskbar_changed || !m_first_frame_drawn) {
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
    
    // Draw Notifications
    {
        std::lock_guard lock(m_mutex);
        int32_t notif_y = display_height() - kTaskbarHeight - 5; // Start drawing above taskbar
        for (const auto& notif : m_notifications) {
            m_toolkit->draw_panel(m_wallpaper_surface, display_width() - 320, notif_y - 60, 300, 50);
            m_toolkit->draw_text(m_wallpaper_surface, display_width() - 310, notif_y - 50, notif.title, desktop::Color{255, 255, 255, 255});
            m_toolkit->draw_text(m_wallpaper_surface, display_width() - 310, notif_y - 30, notif.body, desktop::Color{200, 200, 200, 255});
            notif_y -= 60;
        }
        
        // Draw Launcher
        if (m_launcher_visible) {
            // Clear Launcher surface
            {
                auto map_launcher = m_launcher_surface->map_pixels();
                std::fill(map_launcher.begin(), map_launcher.end(), static_cast<uint8_t>(0));
                m_launcher_surface->unmap_pixels();
            }
            
            m_toolkit->draw_panel(m_launcher_surface, 0, 0, 300, 400);
            m_toolkit->draw_rect(m_launcher_surface, 0, 0, 300, 30, desktop::Color{50, 50, 70, 255});
            m_toolkit->draw_text(m_launcher_surface, 10, 10, "Applications", desktop::Color{255, 255, 255, 255});
            
            // Mock apps
            m_toolkit->draw_button(m_launcher_surface, 10, 50, 280, 40, "Terminal");
            m_toolkit->draw_button(m_launcher_surface, 10, 100, 280, 40, "File Explorer");
            m_toolkit->draw_button(m_launcher_surface, 10, 150, 280, 40, "System Settings");
        }
    }
    
    // Update and render running applications
    {
        std::lock_guard lock(m_mutex);
        for (auto& running_app : m_running_apps) {
            if (running_app.app) {
                running_app.app->update(16.6); // Transitional constant delta time
                if (running_app.visible && running_app.surface) {
                    running_app.app->render(*(running_app.surface));
                }
            }
        }
    }
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
    std::lock_guard lock(m_mutex);
    if (m_focused_window.has_value() && m_focused_window.value() == window_id) {
        m_focused_window = std::nullopt; // safely clear focus
    }
    std::erase_if(m_running_apps, [window_id](const auto& app) { return app.window_id == window_id; });
}

void DesktopShell::clear_running_applications() {
    std::lock_guard lock(m_mutex);
    m_focused_window = std::nullopt; // safely clear focus
    m_running_apps.clear();
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

void DesktopShell::update_window_decoration(uint32_t window_id, bool focused) {
    auto bounds = m_wm->get_decorated_window_bounds(window_id);
    if (bounds.width == 0 || bounds.height == 0) return;

    std::string title = "Window";
    {
        std::lock_guard lock(m_mutex);
        for (const auto& app : m_running_apps) {
            if (app.window_id == window_id && app.app) {
                // If application exposes a title, use it. Defaulting for now.
                // In future, IApplication should expose get_title()
                title = "Application"; 
                break;
            }
        }
    }

    auto dec_surface = m_compositor->create_surface(bounds.width, bounds.height);
    
    desktop::WindowFrameMetrics metrics;
    metrics.decorated_bounds = bounds;
    // DesktopShell doesn't strictly need client_bounds right here for layout generation 
    // unless the layout engine relies on it. But we can fetch it to be thorough.
    if (auto client_geom = m_wm->get_window_geometry(window_id)) {
        metrics.client_bounds = {client_geom->x, client_geom->y, client_geom->width, client_geom->height};
    } else {
        metrics.client_bounds = bounds;
    }
    metrics.margins = m_decorator->get_margins();
    
    desktop::DecorationLayout layout = desktop::calculate_decoration_layout(metrics);
    
    m_decorator->draw_decorations(dec_surface, layout, title, focused, m_wm->get_window_state(window_id), m_theme);
    m_wm->set_window_decoration(window_id, dec_surface, m_decorator->get_margins());
    invalidate_window_region(window_id);
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
