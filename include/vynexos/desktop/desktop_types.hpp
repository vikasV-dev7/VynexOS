#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <span>
#include <string>

namespace vynexos::desktop {

struct Rect {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct Color {
    uint8_t r, g, b, a;
};

struct WindowTheme {
    Color active_title_color;
    Color inactive_title_color;
    Color border_color;
    Color background_color;
    Color text_color;
};

struct WindowMargins {
    int32_t top;
    int32_t bottom;
    int32_t left;
    int32_t right;
};

enum class ResizeEdge {
    None,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

/// Everything the decoration layout needs to know about a window's frame except its size. A window that never
/// receives a style keeps these defaults, which reproduce the original (pre-redesign) frame metrics.
///
/// Geometry model (all relative to the window's *decorated bounds*, which are the client area grown by `margins`):
///   - `resize_inset` is an invisible band at the very edge of the decorated bounds that is hit-testable for resizing
///     but not drawn. The visible frame is the decorated bounds inset by it.
///   - The resize zones form a ring of `resize_inset + border_thickness` around the decorated bounds: the invisible band
///     plus the visible border.
///   - The title bar is the part of the top margin inside the visible frame: `margins.top - resize_inset` tall.
struct FrameStyle {
    WindowMargins margins{0, 0, 0, 0};
    uint32_t border_thickness{5};
    uint32_t resize_inset{0};
    uint32_t button_size{30};
    uint32_t button_spacing{10};
    uint32_t button_margin_top{5};
    uint32_t button_margin_right{10};
};

/// Stacking class of a window. The window manager's single stacking order is sorted by layer, in this order, bottom to
/// top; composition and hit-testing both follow it, so what is drawn on top is what receives the pointer. Within a layer,
/// the most recently raised (focused) window is on top. Raising never moves a window out of its layer.
enum class WindowLayer : uint8_t {
    Desktop = 0,  ///< wallpaper: below everything
    Normal = 1,   ///< application windows (default)
    Panel = 2,    ///< taskbar: above every application window, maximized ones included
    Overlay = 3   ///< launcher, menus: above panels
};

struct WindowFrameMetrics {
    Rect decorated_bounds;
    Rect client_bounds;
    WindowMargins margins;
    uint32_t title_bar_height{40};  // retained for compatibility; the layout derives the title bar from margins.top
    uint32_t border_thickness{5};
    uint32_t button_size{30};
    uint32_t button_spacing{10};
    uint32_t button_margin_top{5};
    uint32_t button_margin_right{10};
    uint32_t resize_inset{0};
};

struct DecorationLayout {
    Rect visible_frame;   ///< the drawn window frame (decorated bounds minus the invisible resize band)
    Rect title_bar;
    Rect client_area;
    Rect close_button;
    Rect minimize_button;
    Rect maximize_button;

    Rect left_border;
    Rect right_border;
    Rect top_border;
    Rect bottom_border;

    Rect top_left_resize;
    Rect top_right_resize;
    Rect bottom_left_resize;
    Rect bottom_right_resize;
};

enum class WindowState {
    Normal,
    Minimized,
    Maximized
};

struct WindowSnapshot {
    uint32_t window_id;
    std::string title;
    WindowState state;
    bool is_visible;
};

enum class DecorationHitRegion {
    None,
    TitleBar,
    CloseButton,
    MinimizeButton,
    MaximizeButton,
    Border,
    ResizeHandle
};

struct HitTestResult {
    uint32_t window_id;
    ResizeEdge edge;
};

// Represents a renderable graphics surface.
class ISurface {
public:
    virtual ~ISurface() = default;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    
    // Polymorphic memory access for drawing
    virtual std::span<uint8_t> map_pixels() = 0;
    virtual void unmap_pixels() = 0;
};

struct RenderLayer {
    Rect bounds;
    Rect clip;
    float opacity{1.0f};
    bool visible{true};
    std::shared_ptr<ISurface> surface;
    uint32_t window_id{0};
};

using SceneGraph = std::vector<RenderLayer>;

} // namespace vynexos::desktop
