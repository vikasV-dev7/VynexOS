#pragma once
#include <optional>
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

namespace vynexos::desktop {

class BasicWindowManager final : public IWindowManager {
public:
    BasicWindowManager(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger);
    ~BasicWindowManager() override = default;

    uint32_t create_window(std::string_view title, WindowGeometry geometry) override;
    void destroy_window(uint32_t window_id) override;
    
    void focus_window(uint32_t window_id) override;
    void move_window(uint32_t window_id, int32_t x, int32_t y) override;
    void resize_window(uint32_t window_id, uint32_t width, uint32_t height) override;
    void set_window_geometry(uint32_t window_id, const WindowGeometry& geometry) override;
    
    void set_work_area(const WindowGeometry& area) override;
    WindowGeometry get_work_area() const override;
    
    std::optional<uint32_t> get_window_at(int32_t x, int32_t y) const override;
    std::optional<HitTestResult> get_resize_handle_at(int32_t x, int32_t y) const override;
    DecorationHitRegion get_decoration_hit_region(uint32_t window_id, int32_t x, int32_t y) const override;
    bool is_window_visible(uint32_t window_id) const override;
    std::optional<WindowGeometry> get_window_geometry(uint32_t window_id) const override;
    std::string get_window_title(uint32_t window_id) const override;
    WindowMargins get_window_margins(uint32_t window_id) const override;
    Rect get_decorated_window_bounds(uint32_t window_id) const override;
    
    void set_window_state(uint32_t window_id, WindowState state) override;
    WindowState get_window_state(uint32_t window_id) const override;
    WindowState get_previous_window_state(uint32_t window_id) const override;
    std::vector<uint32_t> get_visible_windows_z_ordered() const override;
    
    std::vector<WindowSnapshot> get_window_snapshots() const override;
    std::optional<uint32_t> get_active_window() const override;
    
    void set_window_surface(uint32_t window_id, std::shared_ptr<ISurface> surface) override;
    void set_window_decoration(uint32_t window_id, std::shared_ptr<ISurface> decoration, WindowMargins margins) override;
    void set_window_visibility(uint32_t window_id, bool visible) override;
    
    std::vector<uint32_t> get_windows_z_ordered() const override;

    SceneGraph build_scene() const override;

    void set_window_frame_styles(uint32_t window_id, const FrameStyle& normal, const FrameStyle& maximized) override;
    FrameStyle get_window_frame_style(uint32_t window_id) const override;
    void set_window_layer(uint32_t window_id, WindowLayer layer) override;
    WindowLayer get_window_layer(uint32_t window_id) const override;

private:

    struct WindowRecord {
        std::string title;
        WindowGeometry geometry;
        std::optional<WindowGeometry> restore_geometry;
        std::shared_ptr<ISurface> surface;
        std::shared_ptr<ISurface> decoration_surface;
        WindowMargins margins{0, 0, 0, 0};  // margins currently in effect (follow the active style when styles are set)
        FrameStyle frame_normal{};
        std::optional<FrameStyle> frame_maximized;  // set together with frame_normal by set_window_frame_styles()
        bool maximized_style_active{false};
        WindowLayer layer{WindowLayer::Normal};
        bool visible{true};
        WindowState state{WindowState::Normal};
        WindowState previous_state{WindowState::Normal};
        float opacity{1.0f};
        uint32_t min_width{100};
        uint32_t min_height{50};
    };

    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::ILogger> m_logger;
    mutable std::mutex m_mutex;
    
    uint32_t m_next_id{1};
    std::unordered_map<uint32_t, WindowRecord> m_windows;
    std::vector<uint32_t> m_creation_order;
    WindowGeometry m_work_area{0, 0, 1920, 1080};
    std::vector<uint32_t> m_z_order; // front is bottom, back is top
    std::optional<uint32_t> m_focused_window;

    // The style in effect for a window, with the margins actually in use (set_window_decoration may override them).
    [[nodiscard]] static FrameStyle effective_style(const WindowRecord& win);
    [[nodiscard]] static Rect decorated_bounds_of(const WindowRecord& win);
    [[nodiscard]] static DecorationLayout layout_of(const WindowRecord& win);

    // Who is under a point. Visible pixels win over invisible resize bands, and the desktop (wallpaper) is last:
    //   1. the topmost non-desktop window whose VISIBLE frame contains the point;
    //   2. otherwise the topmost non-desktop window whose invisible resize band contains it (`band` = true);
    //   3. otherwise the desktop window.
    // so a window's invisible band never steals clicks from another window's visible area, a panel, or a maximized window.
    struct Hit {
        uint32_t id;
        bool band;
    };
    [[nodiscard]] std::optional<Hit> find_hit_locked(int32_t x, int32_t y) const;
};

} // namespace vynexos::desktop
