#include "desktop_shell.hpp"

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
}

void DesktopShell::launch() {
    // 1. Wallpaper (Fullscreen)
    desktop::WindowGeometry wall_geom{0, 0, 1920, 1080};
    m_wallpaper_id = m_wm->create_window("Wallpaper", wall_geom);
    
    m_wallpaper_buffer.id = m_wallpaper_id;
    m_wallpaper_buffer.x = wall_geom.x;
    m_wallpaper_buffer.y = wall_geom.y;
    m_wallpaper_buffer.width = wall_geom.width;
    m_wallpaper_buffer.height = wall_geom.height;
    m_wallpaper_buffer.pixels.resize(wall_geom.width * wall_geom.height * 4, static_cast<uint8_t>(30)); // Dark grey static background
    
    // 2. Taskbar (Bottom panel)
    desktop::WindowGeometry task_geom{0, 1080 - 40, 1920, 40};
    m_taskbar_id = m_wm->create_window("Taskbar", task_geom);
    
    m_taskbar_buffer.id = m_taskbar_id;
    m_taskbar_buffer.x = task_geom.x;
    m_taskbar_buffer.y = task_geom.y;
    m_taskbar_buffer.width = task_geom.width;
    m_taskbar_buffer.height = task_geom.height;
    m_taskbar_buffer.pixels.resize(task_geom.width * task_geom.height * 4, static_cast<uint8_t>(0)); // Transparent initialization
}

void DesktopShell::update_frame() {
    if (m_wallpaper_id == 0 || m_taskbar_id == 0) return;
    
    // Submit static wallpaper
    m_compositor->submit_buffer(m_wallpaper_buffer);
    
    // Draw Taskbar using toolkit
    m_toolkit->draw_panel(m_taskbar_buffer, 0, 0, m_taskbar_buffer.width, m_taskbar_buffer.height);
    m_toolkit->draw_button(m_taskbar_buffer, 10, 5, 80, 30, "Vynex");
    m_toolkit->draw_text(m_taskbar_buffer, 1850, 15, "12:00", desktop::Color{255, 255, 255, 255});
    
    // Draw Notifications
    {
        std::lock_guard lock(m_mutex);
        int32_t notif_y = m_taskbar_buffer.height - 45; // Start drawing above taskbar
        for (const auto& notif : m_notifications) {
            // We just render text on the taskbar buffer for simplicity (since it spans the screen width)
            // But wait, the taskbar buffer is only 40 pixels high!
            // Instead, we should draw it on the wallpaper buffer, as it covers the full screen.
            // Let's modify the wallpaper buffer safely here (since it's static usually).
            m_toolkit->draw_panel(m_wallpaper_buffer, 1600, notif_y - 60, 300, 50);
            m_toolkit->draw_text(m_wallpaper_buffer, 1610, notif_y - 50, notif.title, desktop::Color{255, 255, 255, 255});
            m_toolkit->draw_text(m_wallpaper_buffer, 1610, notif_y - 30, notif.body, desktop::Color{200, 200, 200, 255});
            notif_y -= 60;
            
            // Re-submit wallpaper buffer to update with notifications
            m_compositor->submit_buffer(m_wallpaper_buffer);
        }
    }
    
    // Submit dynamic taskbar
    m_compositor->submit_buffer(m_taskbar_buffer);
}

} // namespace vynexos::apps
