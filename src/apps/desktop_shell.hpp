#pragma once
#include "vynexos/apps/desktop_shell.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/desktop/notification_service.hpp"
#include <memory>
#include <vector>
#include <mutex>

namespace vynexos::apps {

class DesktopShell final : public IDesktopShell {
public:
    DesktopShell(std::shared_ptr<desktop::IWindowManager> wm,
                 std::shared_ptr<desktop::ICompositor> compositor,
                 std::shared_ptr<desktop::IWidgetToolkit> toolkit,
                 std::shared_ptr<core::IEventBus> event_bus);
    
    void launch() override;
    void update_frame() override;

private:
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    std::shared_ptr<core::IEventBus> m_event_bus;
    
    uint32_t m_wallpaper_id{0};
    uint32_t m_taskbar_id{0};
    
    desktop::WindowBuffer m_wallpaper_buffer;
    desktop::WindowBuffer m_taskbar_buffer;

    std::mutex m_mutex;
    std::vector<desktop::Notification> m_notifications;
};

} // namespace vynexos::apps
