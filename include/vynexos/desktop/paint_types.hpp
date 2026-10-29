#pragma once
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

/// Per-corner radii in pixels. Values are clamped by the painter to half of the shape's shorter side.
struct CornerRadii {
    float top_left{0.0f};
    float top_right{0.0f};
    float bottom_right{0.0f};
    float bottom_left{0.0f};

    [[nodiscard]] static constexpr CornerRadii uniform(float r) noexcept { return {r, r, r, r}; }
    /// Rounds only the top corners (e.g. a title bar sitting on a square-cornered body).
    [[nodiscard]] static constexpr CornerRadii top(float r) noexcept { return {r, r, 0.0f, 0.0f}; }
    /// Rounds only the bottom corners.
    [[nodiscard]] static constexpr CornerRadii bottom(float r) noexcept { return {0.0f, 0.0f, r, r}; }
};

/// A soft drop shadow. `color.a` is the peak opacity; `blur` is the half-width of the falloff band.
struct ShadowSpec {
    float blur{0.0f};
    float offset_x{0.0f};
    float offset_y{0.0f};
    float spread{0.0f};
    Color color{0, 0, 0, 0};
};

/// A two-stop linear gradient across a rectangle (top->bottom or left->right).
struct LinearGradient {
    Color start{0, 0, 0, 255};
    Color end{0, 0, 0, 255};
    bool vertical{true};
    /// Ordered dithering hides 8-bit banding in dark, low-contrast gradients. Deterministic.
    bool dither{true};
};

} // namespace vynexos::desktop
