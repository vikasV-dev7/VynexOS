#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <optional>
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

// Coordinate spaces (the geometry contract)
//
//   Host window      SDL window units. The HAL maps them, through the viewport (aspect-preserving scale + letterbox) and
//                    any high-DPI factor, to desktop coordinates before publishing input; nothing above the HAL sees them.
//   Desktop          The logical desktop (the compositor's framebuffer). All pointer events on the bus, window geometry,
//                    decorated bounds and redraw requests are in desktop coordinates.
//   Client area      WindowGeometry: the window's content, in desktop coordinates; (x, y) is the client origin.
//   Decorated bounds The client area grown by the window's margins (title bar, borders, invisible resize band).
//   Content          Application coordinates: origin at the client area's top-left. An application's surface is exactly
//                    the client area's size, decorations are never part of it, and pointer events it receives are
//                    desktop coordinates minus the client origin, nothing else. It receives them over its client area
//                    (a press where a corner resize grip overlaps the client resizes instead) and, while a press that
//                    began there is held, anywhere; never for presses that began on decorations or elsewhere.
struct WindowGeometry {
    int32_t x, y;
    uint32_t width, height;
};

/// The decorated bounds of a window whose client area is `client` (desktop coordinates).
[[nodiscard]] constexpr Rect decorated_bounds(const WindowGeometry& client, const WindowMargins& margins) noexcept {
    return {client.x - margins.left, client.y - margins.top, client.width + static_cast<uint32_t>(margins.left + margins.right),
            client.height + static_cast<uint32_t>(margins.top + margins.bottom)};
}

/// The client area that fills `outer` once `margins` are taken off (empty if the margins do not fit).
[[nodiscard]] constexpr WindowGeometry client_within(const WindowGeometry& outer, const WindowMargins& margins) noexcept {
    const auto h = static_cast<uint32_t>(margins.left + margins.right);
    const auto v = static_cast<uint32_t>(margins.top + margins.bottom);
    return {outer.x + margins.left, outer.y + margins.top, outer.width > h ? outer.width - h : 0u, outer.height > v ? outer.height - v : 0u};
}

/// Window placement policy: how much of a window must stay reachable, as a width of its title bar.
/// 128 px keeps either the whole control cluster (minimize, maximize, close: 3 x 28 px buttons, gaps and margin = 100 px)
/// plus a grip, or a stretch of title bar wide enough to grab, on screen. (Narrower windows stay entirely on screen.)
inline constexpr int32_t kMinVisibleTitleBar = 128;

/// THE placement policy for a normal window (it moves a window, it never resizes it): `client` moved as little as needed
/// so that its title bar stays reachable inside `work_area` (the desktop minus the taskbar):
///  - vertically, the whole title bar (the top margin) is inside the work area: never above the top edge, never under
///    the taskbar; if the work area is shorter than the title bar, the title bar starts at its top;
///  - horizontally, at least kMinVisibleTitleBar of it is inside the work area (all of it if the window or the work area
///    is narrower), so a window can still be pushed partly off the left or right edge.
/// The body may extend below the work area (it can always be dragged back up by the title bar).
[[nodiscard]] constexpr WindowGeometry constrain_window_position(WindowGeometry client, const WindowMargins& margins,
                                                                 const WindowGeometry& work_area) noexcept {
    const int64_t decorated_w = int64_t{client.width} + margins.left + margins.right;
    const int64_t visible = std::min({int64_t{kMinVisibleTitleBar}, decorated_w, int64_t{work_area.width}});
    const int64_t min_x = int64_t{work_area.x} + visible - decorated_w; // decorated left edge, at least `visible` on screen
    const int64_t max_x = int64_t{work_area.x} + int64_t{work_area.width} - visible;
    const int64_t decorated_x = std::clamp(int64_t{client.x} - margins.left, min_x, max_x);

    const int64_t min_y = work_area.y; // decorated top edge: the title bar fully inside the work area
    const int64_t max_y = std::max(min_y, int64_t{work_area.y} + int64_t{work_area.height} - margins.top);
    const int64_t decorated_y = std::clamp(int64_t{client.y} - margins.top, min_y, max_y);

    client.x = static_cast<int32_t>(decorated_x + margins.left);
    client.y = static_cast<int32_t>(decorated_y + margins.top);
    return client;
}

/// Whether the desktop point (x, y) lies inside the client area.
[[nodiscard]] constexpr bool client_contains(const WindowGeometry& client, int32_t x, int32_t y) noexcept {
    return x >= client.x && y >= client.y && static_cast<int64_t>(x) < static_cast<int64_t>(client.x) + client.width &&
           static_cast<int64_t>(y) < static_cast<int64_t>(client.y) + client.height;
}

class IWindowManager {
public:
    virtual ~IWindowManager() = default;
    
    // Window lifecycle
    virtual uint32_t create_window(std::string_view title, WindowGeometry geometry) = 0;
    virtual void destroy_window(uint32_t window_id) = 0;
    
    // Work Area
    virtual void set_work_area(const WindowGeometry& area) = 0;
    virtual WindowGeometry get_work_area() const = 0;
    
    // Focus and layout
    virtual void focus_window(uint32_t window_id) = 0;
    virtual void move_window(uint32_t window_id, int32_t x, int32_t y) = 0;
    // Size changes are all or nothing: a size whose decorated bounds no surface may have (surface_limits.hpp) is
    // rejected, the window keeps its previous geometry, and false is returned. Sizes below the window's minimum are
    // raised to it. Positions are taken as given (placement policy: constrain_window_position, applied by the shell and
    // the application host where windows are placed).
    virtual bool resize_window(uint32_t window_id, uint32_t width, uint32_t height) = 0;
    virtual bool set_window_geometry(uint32_t window_id, const WindowGeometry& geometry) = 0;
    virtual std::optional<uint32_t> get_window_at(int32_t x, int32_t y) const = 0;
    virtual std::optional<HitTestResult> get_resize_handle_at(int32_t x, int32_t y) const = 0;
    virtual DecorationHitRegion get_decoration_hit_region(uint32_t window_id, int32_t x, int32_t y) const = 0;
    virtual bool is_window_visible(uint32_t window_id) const = 0;
    virtual std::optional<WindowGeometry> get_window_geometry(uint32_t window_id) const = 0;
    virtual std::string get_window_title(uint32_t window_id) const = 0;
    virtual WindowMargins get_window_margins(uint32_t window_id) const = 0;
    virtual Rect get_decorated_window_bounds(uint32_t window_id) const = 0;
    
    // Window State
    virtual void set_window_state(uint32_t window_id, WindowState state) = 0;
    virtual WindowState get_window_state(uint32_t window_id) const = 0;
    virtual WindowState get_previous_window_state(uint32_t window_id) const = 0;
    virtual std::vector<uint32_t> get_visible_windows_z_ordered() const = 0;
    
    // Enumeration
    virtual std::vector<WindowSnapshot> get_window_snapshots() const = 0;
    virtual std::optional<uint32_t> get_active_window() const = 0;
    
    // Surface and visibility
    virtual void set_window_surface(uint32_t window_id, std::shared_ptr<ISurface> surface) = 0;
    virtual void set_window_decoration(uint32_t window_id, std::shared_ptr<ISurface> decoration, WindowMargins margins) = 0;
    virtual void set_window_visibility(uint32_t window_id, bool visible) = 0;
    
    // Returns window IDs ordered back-to-front for the compositor
    virtual std::vector<uint32_t> get_windows_z_ordered() const = 0;

    // Scene Orchestration
    virtual SceneGraph build_scene() const = 0;

    // Frame geometry (non-pure: implementations without chrome keep the original behaviour).
    //
    // A window's frame style describes its margins and resize band for each state. Setting styles makes the window
    // manager use the NORMAL style's margins immediately and switch to the MAXIMIZED style's margins while the window is
    // maximized (so a maximized window can sit flush with the work area, with no borders or resize band).
    virtual void set_window_frame_styles(uint32_t window_id, const FrameStyle& normal, const FrameStyle& maximized) {
        (void)window_id;
        (void)normal;
        (void)maximized;
    }
    /// The frame style currently in effect for the window (the style of its present state), margins included.
    [[nodiscard]] virtual FrameStyle get_window_frame_style(uint32_t window_id) const {
        (void)window_id;
        return FrameStyle{};
    }

    // Stacking class (see WindowLayer). Setting a layer puts the window on top of that layer. The stacking order is the
    // composition order and the hit-testing order: the topmost window whose decorated bounds contain a point receives
    // it, its invisible resize band included (so a window can be resized from just outside its edge even over the window
    // below it, but never over a window above it, a panel or an overlay).
    virtual void set_window_layer(uint32_t window_id, WindowLayer layer) {
        (void)window_id;
        (void)layer;
    }
    [[nodiscard]] virtual WindowLayer get_window_layer(uint32_t window_id) const {
        (void)window_id;
        return WindowLayer::Normal;
    }
};

} // namespace vynexos::desktop
