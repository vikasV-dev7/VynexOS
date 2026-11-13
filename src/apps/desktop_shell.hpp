#pragma once
#include "vynexos/apps/desktop_shell.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include "vynexos/desktop/window_decorator.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/desktop/notification_service.hpp"
#include "vynexos/hal/input_driver.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>
#include <algorithm>

namespace vynexos::apps {

class DesktopShell final : public IDesktopShell {
public:
    DesktopShell(std::shared_ptr<desktop::IWindowManager> wm,
                 std::shared_ptr<desktop::ICompositor> compositor,
                 std::shared_ptr<desktop::IWidgetToolkit> toolkit,
                 std::shared_ptr<desktop::IWindowDecorator> decorator,
                 std::shared_ptr<desktop::ITaskbar> taskbar,
                 std::shared_ptr<core::IEventBus> event_bus);
    /// Unsubscribes every handler (they capture this): when it returns no handler is running or will run again.
    ~DesktopShell() override;
    DesktopShell(const DesktopShell&) = delete;
    DesktopShell& operator=(const DesktopShell&) = delete;
    
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
    std::shared_ptr<desktop::ITaskbar> taskbar() override { return m_taskbar; }
    void set_application_host(IApplicationHost* host) override { m_host = host; }

private:
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    std::shared_ptr<desktop::IWindowDecorator> m_decorator;
    std::shared_ptr<desktop::ITaskbar> m_taskbar;
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::vector<core::IEventBus::SubscriptionId> m_subscriptions;

    desktop::WindowTheme m_theme;

    void update_window_decoration(uint32_t window_id, bool focused);
    void invalidate_window_region(uint32_t window_id);

    // Chrome is drawn for every window the shell does not own (wallpaper, taskbar, launcher), not only registered
    // applications. A window's chrome is redrawn only when what it shows changes; the stamp records what was drawn.
    // The surface is kept and reused: a change of hover or pressed control repaints just the affected controls in
    // place, anything else repaints the same surface, and only a new size allocates a new one.
    struct ChromeStamp {
        bool focused{false};
        desktop::WindowState state{desktop::WindowState::Normal};
        uint32_t width{0};
        uint32_t height{0};
        std::string title;
        desktop::DecorationHitRegion hovered{desktop::DecorationHitRegion::None};
        desktop::DecorationHitRegion pressed{desktop::DecorationHitRegion::None};
        std::shared_ptr<desktop::ISurface> surface;
    };
    std::unordered_map<uint32_t, ChromeStamp> m_chrome;
    void sync_window_chrome(const desktop::WindowSnapshot& snapshot);
    void refresh_chrome(uint32_t window_id);

    // Application content reaches the screen only through here: an application's surface is re-rendered, and the
    // compositor told which parts to redraw, when the application reports a change (IApplication::take_invalidated_region)
    // or its surface has not been painted yet. The stamp records which surface was last painted for a window.
    struct ContentStamp {
        const desktop::ISurface* surface{nullptr};
        uint32_t width{0};
        uint32_t height{0};
    };
    std::unordered_map<uint32_t, ContentStamp> m_content; // guarded by m_mutex
    void present_application_content(desktop::RunningApplication& app);
    void fit_application_surface(desktop::RunningApplication& app, const desktop::WindowGeometry& client);

    // Pointer input for applications, in content coordinates (see the geometry contract in window_manager.hpp).
    std::optional<uint32_t> m_app_pointer_owner; // window whose application holds the pointer (pressed in its content); guarded by m_mutex
    void route_pointer_to_application(const hal::MouseEvent& mouse, bool pressed, bool released);
    bool deliver_pointer_locked(uint32_t window_id, const hal::MouseEvent& mouse);

    // Keyboard input goes to the focused application only (not while the launcher is open, not to a minimized or closed
    // one). A key release goes to the application that received its press, and only while that one still has focus.
    std::unordered_map<uint32_t, uint32_t> m_key_presses; // key code -> window that received the press; guarded by m_mutex
    desktop::RunningApplication* keyboard_target_locked();
    void route_key(const core::Event& ev, const hal::KeyEvent& key);
    void route_text(const core::Event& ev);

    // Launcher: a desktop component (no decorations, never focused). Its entries come from the application host and
    // act on release over the entry that was pressed; Escape or a press anywhere else dismisses it.
    IApplicationHost* m_host{nullptr};
    std::optional<size_t> m_launcher_press; // entry pressed, until the release; guarded by m_mutex
    void set_launcher_visible(bool visible);

    // Moves a window to (x, y), or as near as the placement policy allows (title bar reachable in the work area).
    void place_window(uint32_t window_id, int32_t x, int32_t y);

    // Focus succession: when the focused window closes or is minimized, the topmost remaining application window takes the
    // focus (nothing does if none is left). Restoring a window focuses it.
    std::optional<uint32_t> next_focus_candidate(uint32_t leaving);
    void focus_next_after(uint32_t leaving);
    std::optional<size_t> launcher_entry_at(int32_t x, int32_t y) const;
    static desktop::Rect launcher_entry_rect(size_t index) {
        return {kLauncherEntryX, kLauncherEntryTop + static_cast<int32_t>(index) * kLauncherEntryStep, kLauncherEntryWidth, kLauncherEntryHeight};
    }
    void invalidate_content_region(const desktop::WindowGeometry& client, const desktop::ISurface& surface, const desktop::Rect& region);

    // Pointer interaction with the title-bar controls (minimize, maximize, close). A press captures the pointer for
    // that control: it shows as pressed only while the pointer is over it, activates on release over it, and is
    // abandoned if the pointer is released elsewhere, the window goes away or changes state, or input is cancelled.
    struct ControlTarget {
        std::optional<uint32_t> window_id;
        desktop::DecorationHitRegion control{desktop::DecorationHitRegion::None};
        bool operator==(const ControlTarget&) const = default;
    };
    struct ChromePointer {
        ControlTarget hover;                 // control under the pointer while no button is held
        ControlTarget press;                 // control that captured the pointer
        bool press_inside{false};            // pointer currently over the captured control
        desktop::WindowState press_state{desktop::WindowState::Normal};  // a change of state interrupts the press
    };
    ChromePointer m_pointer;                 // guarded by m_mutex
    bool m_input_suppressed{false};          // after a cancel: ignore held buttons until everything is released

    ControlTarget control_at(int32_t x, int32_t y) const;
    desktop::DecorationVisualState chrome_visual_locked(uint32_t window_id, bool focused, desktop::WindowState state) const;
    void set_hover(const ControlTarget& target);
    void begin_control_press(const ControlTarget& target);
    void track_control_press(int32_t x, int32_t y);
    void release_control_press(int32_t x, int32_t y);
    void activate_control(uint32_t window_id, desktop::DecorationHitRegion control);
    void validate_captures();
    void cancel_interaction();
    bool is_shell_window(uint32_t window_id) const {
        return window_id == m_wallpaper_id || window_id == m_taskbar_id || window_id == m_launcher_id ||
               std::any_of(m_notification_views.begin(), m_notification_views.end(), [&](const NotificationView& v) { return v.window == window_id; });
    }

    // Notifications: one Overlay window per notification (see sync_notifications).
    struct NotificationView {
        uint32_t id;     // the notification
        uint32_t window; // its window (0: none, its surface was refused)
    };
    std::vector<NotificationView> m_notification_views; // frame/input thread only
    static constexpr uint32_t kNotificationWidth = 300;
    static constexpr uint32_t kNotificationHeight = 50;
    static constexpr int32_t kNotificationStep = 60;
    desktop::Rect notification_rect(size_t index) const;
    void sync_notifications();
    
    // Desktop layout derives from the compositor's logical size (no hard-coded resolution).
    static constexpr int32_t kTaskbarHeight = 40;
    static constexpr int32_t kLauncherButtonX = 10;
    static constexpr int32_t kLauncherButtonY = 5;
    static constexpr int32_t kLauncherButtonWidth = 80;
    static constexpr int32_t kLauncherButtonHeight = 30;
    static constexpr int32_t kLauncherEntryX = 10;       // launcher entries, in launcher coordinates
    static constexpr int32_t kLauncherEntryTop = 50;
    static constexpr int32_t kLauncherEntryStep = 50;
    static constexpr uint32_t kLauncherEntryWidth = 280;
    static constexpr uint32_t kLauncherEntryHeight = 40;
    hal::DisplaySize m_display_size{hal::kDefaultDisplaySize};
    int32_t display_width() const { return static_cast<int32_t>(m_display_size.width); }
    int32_t display_height() const { return static_cast<int32_t>(m_display_size.height); }

    uint32_t m_wallpaper_id{0};
    uint32_t m_taskbar_id{0};
    uint32_t m_launcher_id{0};
    
    std::shared_ptr<desktop::ISurface> m_wallpaper_surface;
    std::shared_ptr<desktop::ISurface> m_taskbar_surface;
    std::shared_ptr<desktop::ISurface> m_launcher_surface;
    
    bool m_launcher_visible{false};
    bool m_first_frame_drawn{false};

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

    struct ResizeState {
        std::optional<uint32_t> window_id;
        desktop::ResizeEdge edge{desktop::ResizeEdge::None};
        int32_t initial_mouse_x{0};
        int32_t initial_mouse_y{0};
        desktop::WindowGeometry initial_geometry;
        bool pointer_captured{false};
    };
    ResizeState m_resize_state;
    
    uint8_t m_last_mouse_state{0};
    int32_t m_cursor_x{0};
    int32_t m_cursor_y{0};
};

} // namespace vynexos::apps
