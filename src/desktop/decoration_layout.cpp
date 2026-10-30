#include "vynexos/desktop/decoration_layout.hpp"
#include <algorithm>

namespace vynexos::desktop {

namespace {

// Smallest area of the title bar that must remain for the window title when buttons are fitted.
constexpr uint32_t kMinTitleRegion = 32;

[[nodiscard]] uint32_t sub_clamped(uint32_t a, uint32_t b) noexcept { return a > b ? a - b : 0; }
[[nodiscard]] uint32_t non_negative(int32_t v) noexcept { return v > 0 ? static_cast<uint32_t>(v) : 0; }
[[nodiscard]] int32_t to_i32(uint32_t v) noexcept { return static_cast<int32_t>(v); }

} // namespace

WindowFrameMetrics make_frame_metrics(const FrameStyle& style, const Rect& decorated_bounds, const Rect& client_bounds) {
    WindowFrameMetrics m;
    m.decorated_bounds = decorated_bounds;
    m.client_bounds = client_bounds;
    m.margins = style.margins;
    m.border_thickness = style.border_thickness;
    m.resize_inset = style.resize_inset;
    m.button_size = style.button_size;
    m.button_spacing = style.button_spacing;
    m.button_margin_top = style.button_margin_top;
    m.button_margin_right = style.button_margin_right;
    return m;
}

DecorationLayout calculate_decoration_layout(const WindowFrameMetrics& metrics) {
    DecorationLayout layout{};

    const uint32_t width = metrics.decorated_bounds.width;
    const uint32_t height = metrics.decorated_bounds.height;
    const uint32_t half_min = std::min(width, height) / 2;

    // --- Visible frame: the bounds minus the invisible resize band ----------------------------------------------------
    const uint32_t inset = std::min(metrics.resize_inset, half_min);
    const uint32_t frame_w = width - 2 * inset;
    const uint32_t frame_h = height - 2 * inset;
    layout.visible_frame = {to_i32(inset), to_i32(inset), frame_w, frame_h};

    // --- Client area and title bar ------------------------------------------------------------------------------------
    const uint32_t m_top = non_negative(metrics.margins.top);
    const uint32_t m_bottom = non_negative(metrics.margins.bottom);
    const uint32_t m_left = non_negative(metrics.margins.left);
    const uint32_t m_right = non_negative(metrics.margins.right);
    layout.client_area = {to_i32(std::min(m_left, width)), to_i32(std::min(m_top, height)), sub_clamped(width, m_left + m_right),
                          sub_clamped(height, m_top + m_bottom)};

    const uint32_t title_h = std::min(sub_clamped(m_top, inset), frame_h);
    layout.title_bar = {to_i32(inset), to_i32(inset), frame_w, title_h};

    // --- Resize ring ----------------------------------------------------------------------------------------------------
    const uint32_t ring = std::min(inset + metrics.border_thickness, half_min);
    layout.top_border = {0, 0, width, ring};
    layout.bottom_border = {0, to_i32(height - ring), width, ring};
    layout.left_border = {0, to_i32(ring), ring, sub_clamped(height, 2 * ring)};
    layout.right_border = {to_i32(width - ring), to_i32(ring), ring, sub_clamped(height, 2 * ring)};

    // Corner squares are easier to hit than the bare ring; they extend along both adjoining edges.
    const uint32_t corner = std::min(2 * ring, half_min);
    layout.top_left_resize = {0, 0, corner, corner};
    layout.top_right_resize = {to_i32(width - corner), 0, corner, corner};
    layout.bottom_left_resize = {0, to_i32(height - corner), corner, corner};
    layout.bottom_right_resize = {to_i32(width - corner), to_i32(height - corner), corner, corner};

    // --- Title-bar buttons ----------------------------------------------------------------------------------------------
    const uint32_t size = metrics.button_size;
    const uint32_t gap = metrics.button_spacing;
    const uint32_t margin_right = metrics.button_margin_right;
    if (title_h > 0 && size > 0 && metrics.button_margin_top + size <= title_h) {
        // How many buttons fit while keeping a usable title region? Drop minimize first, then maximize, then close.
        uint32_t count = 0;
        for (uint32_t k = 3; k >= 1; --k) {
            const uint32_t needed = margin_right + k * size + (k - 1) * gap + kMinTitleRegion;
            if (frame_w >= needed) {
                count = k;
                break;
            }
        }
        const int32_t y = to_i32(inset + metrics.button_margin_top);
        int32_t x = to_i32(inset + frame_w) - to_i32(margin_right) - to_i32(size);
        auto place = [&](Rect& r) {
            r = {x, y, size, size};
            x -= to_i32(size + gap);
        };
        if (count >= 1) place(layout.close_button);
        if (count >= 2) place(layout.maximize_button);
        if (count >= 3) place(layout.minimize_button);
    }

    return layout;
}

} // namespace vynexos::desktop
