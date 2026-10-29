#pragma once
#include <string_view>
#include <vector>
#include <cstdint>

#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/paint_types.hpp"
#include "vynexos/desktop/theme.hpp"
#include <memory>
#include <utility>

namespace vynexos::desktop {

class IWidgetToolkit {
public:
    virtual ~IWidgetToolkit() = default;

    // New Native Surface Interface
    virtual void draw_rect(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) = 0;
    virtual void draw_text(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, std::string_view text, Color color) = 0;
    virtual void draw_button(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) = 0;
    virtual void draw_panel(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

    // Extended drawing (anti-aliased shapes, depth, design tokens).
    // These are deliberately NOT pure virtual: existing toolkit implementations and test doubles keep compiling,
    // and fall back to the square-cornered primitives above.

    /// Filled rectangle with per-corner radii. Fallback: a square-cornered draw_rect.
    virtual void draw_rounded_rect(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii, Color color) {
        (void)radii;
        draw_rect(std::move(surface), rect.x, rect.y, rect.width, rect.height, color);
    }

    /// Border drawn inside `rect`. Fallback: four square-cornered edges (radii are ignored).
    virtual void draw_rounded_border(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii, float thickness,
                                     Color color) {
        (void)radii;
        const auto t = static_cast<uint32_t>(thickness < 1.0f ? 1.0f : thickness);
        if (rect.width <= 2 * t || rect.height <= 2 * t) {
            draw_rect(surface, rect.x, rect.y, rect.width, rect.height, color);
            return;
        }
        draw_rect(surface, rect.x, rect.y, rect.width, t, color);
        draw_rect(surface, rect.x, rect.y + static_cast<int32_t>(rect.height - t), rect.width, t, color);
        draw_rect(surface, rect.x, rect.y + static_cast<int32_t>(t), t, rect.height - 2 * t, color);
        draw_rect(surface, rect.x + static_cast<int32_t>(rect.width - t), rect.y + static_cast<int32_t>(t), t, rect.height - 2 * t, color);
    }

    /// Rectangle filled with a two-stop gradient. Fallback: the start colour.
    virtual void draw_gradient_rect(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii,
                                    const LinearGradient& gradient) {
        (void)radii;
        draw_rect(std::move(surface), rect.x, rect.y, rect.width, rect.height, gradient.start);
    }

    /// Soft drop shadow around a box. Fallback: no shadow (depth is optional decoration).
    virtual void draw_shadow(std::shared_ptr<ISurface> surface, const Rect& box, const CornerRadii& radii, const ShadowSpec& shadow) {
        (void)surface;
        (void)box;
        (void)radii;
        (void)shadow;
    }

    /// The design tokens components should read colours, spacing and sizes from.
    [[nodiscard]] virtual const Theme& theme() const { return default_theme(); }
};

} // namespace vynexos::desktop
