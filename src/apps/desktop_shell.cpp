#include "desktop_shell.hpp"
#include "vynexos/hal/input_driver.hpp"

namespace vynexos::apps {

DesktopShell::DesktopShell(std::shared_ptr<desktop::IWindowManager> wm,
                           std::shared_ptr<desktop::ICompositor> compositor,
                           std::shared_ptr<desktop::IWidgetToolkit> toolkit,
                           std::shared_ptr<core::IEventBus> event_bus)
    : m_wm(std::move(wm)), m_compositor(std::move(compositor)), 
      m_toolkit(std::move(toolkit)), m_event_bus(std::move(event_bus)) {
      
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

    m_event_bus->subscribe("HAL_INPUT_MOUSE", [this](std::shared_ptr<const core::Event> ev) {
        if (ev->payload.type() == typeid(hal::MouseEvent)) {
            auto mouse_ev = std::any_cast<hal::MouseEvent>(ev->payload);
            bool clicked = (mouse_ev.button_state != 0 && m_last_mouse_state == 0);
            m_last_mouse_state = mouse_ev.button_state;
            
            if (clicked) { // Click
                // Check if Vynex button bounds clicked: x in [10, 90], y in [1045, 1075]
                if (mouse_ev.x >= 10 && mouse_ev.x <= 90 && mouse_ev.y >= 1045 && mouse_ev.y <= 1075) {
                    std::lock_guard lock(m_mutex);
                    m_launcher_visible = !m_launcher_visible;
                    m_wm->set_window_visibility(m_launcher_id, m_launcher_visible);
                }
            }
        }
    });
}

void DesktopShell::launch() {
    // 1. Wallpaper (Fullscreen)
    desktop::WindowGeometry wall_geom{0, 0, 1920, 1080};
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
    desktop::WindowGeometry task_geom{0, 1080 - 40, 1920, 40};
    m_taskbar_id = m_wm->create_window("Taskbar", task_geom);
    
    m_taskbar_surface = m_compositor->create_surface(task_geom.width, task_geom.height);
    auto map_task = m_taskbar_surface->map_pixels();
    std::fill(map_task.begin(), map_task.end(), static_cast<uint8_t>(0));
    m_taskbar_surface->unmap_pixels();
    m_wm->set_window_surface(m_taskbar_id, m_taskbar_surface);
    
    // 3. Launcher Window
    desktop::WindowGeometry launcher_geom{10, 1080 - 40 - 400, 300, 400};
    m_launcher_id = m_wm->create_window("Launcher", launcher_geom);
    
    m_launcher_surface = m_compositor->create_surface(launcher_geom.width, launcher_geom.height);
    m_wm->set_window_surface(m_launcher_id, m_launcher_surface);
    m_wm->set_window_visibility(m_launcher_id, false);
}

void DesktopShell::update_frame() {
    if (m_wallpaper_id == 0 || m_taskbar_id == 0) return;
    
    // Clear Taskbar surface
    {
        auto map_task = m_taskbar_surface->map_pixels();
        std::fill(map_task.begin(), map_task.end(), static_cast<uint8_t>(0));
        m_taskbar_surface->unmap_pixels();
    }
    
    // Clear Wallpaper surface to base color
    {
        auto map_wall = m_wallpaper_surface->map_pixels();
        for (size_t i = 0; i < map_wall.size(); i += 4) {
            map_wall[i] = 30;
            map_wall[i+1] = 30;
            map_wall[i+2] = 30;
            map_wall[i+3] = 255;
        }
        m_wallpaper_surface->unmap_pixels();
    }
    
    // Draw Taskbar using toolkit (native ISurface interface)
    m_toolkit->draw_panel(m_taskbar_surface, 0, 0, 1920, 40);
    m_toolkit->draw_button(m_taskbar_surface, 10, 5, 80, 30, "Vynex");
    m_toolkit->draw_text(m_taskbar_surface, 1850, 15, "12:00", desktop::Color{255, 255, 255, 255});
    
    // Draw Notifications
    {
        std::lock_guard lock(m_mutex);
        int32_t notif_y = 1080 - 45; // Start drawing above taskbar
        for (const auto& notif : m_notifications) {
            m_toolkit->draw_panel(m_wallpaper_surface, 1600, notif_y - 60, 300, 50);
            m_toolkit->draw_text(m_wallpaper_surface, 1610, notif_y - 50, notif.title, desktop::Color{255, 255, 255, 255});
            m_toolkit->draw_text(m_wallpaper_surface, 1610, notif_y - 30, notif.body, desktop::Color{200, 200, 200, 255});
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
}

} // namespace vynexos::apps
