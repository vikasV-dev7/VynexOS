#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/paint_types.hpp"

namespace vynexos::desktop {

/**
 * @brief Anti-aliased 2D painter for RGBA8 software surfaces.
 *
 * Maps the surface once for its lifetime (RAII), so a whole widget can be drawn with a single map/unmap.
 *
 * Conventions
 *  - Pixels are straight (non-premultiplied) RGBA, byte order R,G,B,A - the format the compositor consumes.
 *  - Blending is correct straight-alpha "source over", including onto transparent or translucent
 *    destinations (e.g. decoration surfaces), where the destination alpha is composited, not clamped.
 *  - Integer rectangles are pixel-aligned; float coordinates (circles, lines) address pixel centres at +0.5.
 *  - Every operation is clipped to the surface and the current clip stack, and is safe for arbitrary,
 *    negative, huge or non-finite arguments: it draws what is visible and never writes out of bounds.
 *  - No heap allocation is performed by any drawing call.
 *
 * Not thread-safe; a Painter is a short-lived, stack-scoped object.
 */
class Painter {
public:
    static constexpr size_t kMaxClipDepth = 8;

    explicit Painter(ISurface& surface);
    ~Painter();

    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;
    Painter(Painter&&) = delete;
    Painter& operator=(Painter&&) = delete;

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

    // --- Clipping -------------------------------------------------------------------------------------
    /// Intersects the current clip with `rect` and pushes the result. Returns false (and pushes nothing) when the
    /// stack is full. An empty intersection is valid: subsequent drawing is simply discarded.
    bool push_clip(const Rect& rect) noexcept;
    /// Restores the previous clip. Popping the base clip (the surface bounds) is a no-op.
    void pop_clip() noexcept;
    [[nodiscard]] Rect clip_bounds() const noexcept;

    /// RAII helper: pushes on construction, pops on destruction (only if the push succeeded).
    class ClipScope {
    public:
        ClipScope(Painter& painter, const Rect& rect) noexcept : m_painter(painter), m_pushed(painter.push_clip(rect)) {}
        ~ClipScope() {
            if (m_pushed) m_painter.pop_clip();
        }
        ClipScope(const ClipScope&) = delete;
        ClipScope& operator=(const ClipScope&) = delete;
        [[nodiscard]] bool pushed() const noexcept { return m_pushed; }

    private:
        Painter& m_painter;
        bool m_pushed;
    };

    // --- Rectangles -----------------------------------------------------------------------------------
    void fill_rect(const Rect& rect, Color color) noexcept;
    void fill_rounded_rect(const Rect& rect, const CornerRadii& radii, Color color) noexcept;
    void fill_rounded_rect(const Rect& rect, const CornerRadii& radii, const LinearGradient& gradient) noexcept;
    /// Border drawn inside `rect` (the outer edge is the rect's edge). `thickness` in pixels.
    void stroke_rounded_rect(const Rect& rect, const CornerRadii& radii, float thickness, Color color) noexcept;

    /// A filled box with a border, drawn in ONE pass from ONE rectangle: the fill covers the whole rounded shape and
    /// the border (`border_thickness` pixels, inside `rect`) is composited over it, so the two always share the same
    /// outer edge and corner geometry and nothing of the background can show between them, at any size or radius.
    ///  - Each pixel is blended once with the shape's coverage, so the anti-aliased edge carries the border colour only
    ///    (a separate fill and stroke would leave a fringe of fill colour outside the border).
    ///  - A zero/negative/non-finite thickness or a transparent border draws the fill alone; a transparent fill draws
    ///    the border alone; a border at least half as thick as the box covers all of it.
    ///  - The inner edge of the border has the radius `radius - border_thickness` (concentric corners).
    void draw_box(const Rect& rect, const CornerRadii& radii, Color fill, float border_thickness, Color border) noexcept;

    // --- Curves and lines -----------------------------------------------------------------------------
    void fill_circle(float cx, float cy, float radius, Color color) noexcept;
    /// Ring whose outer edge is at `radius`.
    void stroke_circle(float cx, float cy, float radius, float thickness, Color color) noexcept;
    /// Line segment with round caps. Axis-aligned lines cost O(length * thickness).
    void draw_line(float x0, float y0, float x1, float y1, float thickness, Color color) noexcept;

    // --- Gradients ------------------------------------------------------------------------------------
    void fill_gradient(const Rect& rect, const LinearGradient& gradient) noexcept;
    /// Radial gradient inside the circle (cx, cy, radius): `inner` at the centre blending to `outer` at the rim.
    /// Composited over existing pixels, so an `outer` alpha of 0 produces a soft glow.
    void fill_radial_gradient(float cx, float cy, float radius, Color inner, Color outer, bool dither = true) noexcept;

    // --- Masks (text) ---------------------------------------------------------------------------------
    /// Blends `color` through an 8-bit coverage mask (a glyph bitmap, for instance) whose top-left pixel lands at
    /// (x, y). `stride` is the byte distance between mask rows and must be >= `width`. The mask must hold
    /// `(height - 1) * stride + width` bytes. Clipped like every other primitive; a null mask, an empty size, a
    /// stride smaller than the width, or an origin beyond +-2^28 draws nothing.
    void draw_coverage_mask(int32_t x, int32_t y, uint32_t width, uint32_t height, const uint8_t* coverage, size_t stride,
                            Color color) noexcept;

    // --- Depth ----------------------------------------------------------------------------------------
    /// Soft shadow for a rounded box. With `cut_out` (default) the box interior is left untouched so a translucent
    /// window never shows a dark rectangle through itself. Cost is proportional to the shadow ring's area.
    void draw_shadow(const Rect& box, const CornerRadii& radii, const ShadowSpec& shadow, bool cut_out = true) noexcept;

private:
    ISurface& m_surface;
    std::span<uint8_t> m_pixels;
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::array<Rect, kMaxClipDepth> m_clips{};
    size_t m_clip_depth{0};
};

} // namespace vynexos::desktop
