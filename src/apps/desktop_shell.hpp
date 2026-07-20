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
    
    void add_running_application(desktop::RunningApplication&& app) override;
    void remove_running_application(uint32_t window_id) override;
    void clear_running_applications() override;
    size_t running_application_count() const override;

    void focus_window(uint32_t window_id) override;
    void clear_focus() override;
    std::optional<uint32_t> focused_window() const override;
    desktop::RunningApplication* focused_application() override;
    bool has_focus() const override;

private:
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    std::shared_ptr<core::IEventBus> m_event_bus;
    
    uint32_t m_wallpaper_id{0};
    uint32_t m_taskbar_id{0};
    uint32_t m_launcher_id{0};
    
    std::shared_ptr<desktop::ISurface> m_wallpaper_surface;
    std::shared_ptr<desktop::ISurface> m_taskbar_surface;
    std::shared_ptr<desktop::ISurface> m_launcher_surface;
    
    bool m_launcher_visible{false};

    std::mutex m_mutex;
    std::vector<desktop::Notification> m_notifications;
    std::vector<desktop::RunningApplication> m_running_apps;
    std::optional<uint32_t> m_focused_window;
    
    struct DragState {
        std::optional<uint32_t> window_id;
        int32_t initial_mouse_x{0};
        int32_t initial_mouse_y{0};
        int32_t initial_window_x{0};
        int32_t initial_window_y{0};
        bool pointer_captured{false};
    };
    DragState m_drag_state;
    
    uint8_t m_last_mouse_state{0};
};

} // namespace vynexos::apps
