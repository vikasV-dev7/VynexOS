#pragma once
#include <string_view>
#include <vector>
#include <cstdint>

#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/paint_types.hpp"
#include "vynexos/desktop/text_renderer.hpp"
#include "vynexos/desktop/theme.hpp"
#include <memory>
#include <string>
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

    // Typography. Positions are in pixels; the y coordinate is the text BASELINE (not a top edge). Text is a single
    // line of UTF-8; see text_charset.hpp for exactly which characters are supported. Like the extended drawing
    // above, these are not pure virtual: an implementation without a font engine inherits the 8x8-bitmap-era
    // fallbacks below (fixed 8 px advance per byte), so existing toolkits and mocks keep working.

    /// Draws anti-aliased text with the pen starting at (x, baseline_y).
    virtual void draw_text_styled(std::shared_ptr<ISurface> surface, int32_t x, int32_t baseline_y, std::string_view utf8, const TextStyle& style,
                                  Color color) {
        (void)style;
        // Legacy y is the top of an 8 px cell, about 9 px above the baseline (64-bit: baseline_y may be extreme).
        const int64_t top = int64_t{baseline_y} - 9;
        draw_text(std::move(surface), x, static_cast<int32_t>(top < INT32_MIN ? INT32_MIN : top), utf8, color);
    }

    /// Width and line metrics of a run of text in the given style.
    [[nodiscard]] virtual TextMetrics measure_text(std::string_view utf8, const TextStyle& style) const {
        TextMetrics m;
        m.width = 8.0f * static_cast<float>(utf8.size());
        m.ascent = 8.0f;
        m.line_height = style.line_height_px > 0.0f ? style.line_height_px : 8.0f;
        m.glyphs = utf8.size();
        return m;
    }

    /// Ascent, descent, cap height and x-height for aligning text inside boxes.
    [[nodiscard]] virtual FontMetrics font_metrics(const TextStyle& style) const {
        FontMetrics fm;
        fm.ascent = 8.0f;
        fm.line_height = style.line_height_px > 0.0f ? style.line_height_px : 8.0f;
        fm.cap_height = 8.0f;
        fm.x_height = 6.0f;
        return fm;
    }

    /// Bytes of the longest prefix of `utf8` that fits in `max_width` pixels (never splits a UTF-8 sequence in a
    /// real implementation; the fallback counts bytes).
    [[nodiscard]] virtual size_t fit_text_prefix(std::string_view utf8, const TextStyle& style, float max_width) const {
        (void)style;
        if (!(max_width >= 0.0f)) return 0;
        const float cells = max_width / 8.0f;
        return cells >= static_cast<float>(utf8.size()) ? utf8.size() : static_cast<size_t>(cells);
    }

    /// Draws text shortened with an ellipsis if it would be wider than `max_width`; returns the width drawn.
    virtual float draw_text_ellipsized(std::shared_ptr<ISurface> surface, int32_t x, int32_t baseline_y, std::string_view utf8, const TextStyle& style,
                                       Color color, float max_width) {
        const size_t fit = fit_text_prefix(utf8, style, max_width);
        if (fit >= utf8.size()) {
            draw_text_styled(std::move(surface), x, baseline_y, utf8, style, color);
            return measure_text(utf8, style).width;
        }
        const size_t keep = fit >= 3 ? fit - 3 : fit;
        std::string shortened(utf8.substr(0, keep));
        if (fit >= 3) shortened += "...";
        draw_text_styled(std::move(surface), x, baseline_y, shortened, style, color);
        return measure_text(shortened, style).width;
    }
};

} // namespace vynexos::desktop
