#include "vynexos/desktop/components.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vynexos::desktop {

namespace {

constexpr Color kNone{0, 0, 0, 0};

[[nodiscard]] int32_t clamp_i32(int64_t v) noexcept {
    return static_cast<int32_t>(std::clamp<int64_t>(v, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
}

[[nodiscard]] uint32_t clamp_u32(int64_t v) noexcept {
    return static_cast<uint32_t>(std::clamp<int64_t>(v, 0, std::numeric_limits<uint32_t>::max()));
}

/// `rect` grown by `by` pixels on every side (shrunk when negative), in 64-bit arithmetic: never wraps, and a rectangle
/// shrunk past nothing is empty.
[[nodiscard]] Rect grown(const Rect& rect, int32_t by) noexcept {
    const int64_t w = int64_t{rect.width} + 2 * int64_t{by};
    const int64_t h = int64_t{rect.height} + 2 * int64_t{by};
    if (w <= 0 || h <= 0) return {rect.x, rect.y, 0, 0};
    return {clamp_i32(int64_t{rect.x} - by), clamp_i32(int64_t{rect.y} - by), clamp_u32(w), clamp_u32(h)};
}

/// A single line of text inside `box`, vertically centred on the capitals and never drawn outside the box. Left aligned
/// at `pad`, or centred when `centred`; shortened with an ellipsis only when it cannot fit. The padding is a preference,
/// not a reason to cut text: a label wider than the padded space may use the box down to `min_pad` from its edges.
void draw_label(Painter& painter, const TextRenderer* text, const Rect& box, std::string_view label, const TextStyle& style,
                Color color, int32_t pad, int32_t min_pad, bool centred) noexcept {
    if (text == nullptr || label.empty() || color.a == 0 || box.width == 0 || box.height == 0) return;
    const float width = text->measure(label, style).width;
    int64_t inset = pad;
    if (static_cast<float>(int64_t{box.width} - 2 * inset) < width) inset = std::min<int64_t>(pad, min_pad);
    const int64_t available = int64_t{box.width} - 2 * inset;
    if (available <= 0) return;
    const Painter::ClipScope clip(painter, box);
    if (!clip.pushed()) return;

    const FontMetrics fm = text->font_metrics(style);
    const int32_t baseline = clamp_i32(int64_t{box.y} + static_cast<int64_t>((static_cast<float>(box.height) + fm.cap_height) * 0.5f));
    const float max_width = static_cast<float>(std::min<int64_t>(available, 1 << 24));
    float x = static_cast<float>(box.x) + static_cast<float>(inset);
    if (centred && width < max_width) x += std::floor((max_width - width) * 0.5f);
    text->draw_ellipsized(painter, x, baseline, label, style, color, max_width);
}

} // namespace

// ----------------------------------------------------------------------------------------------------------------------
// State model
// ----------------------------------------------------------------------------------------------------------------------

Color state_fill(const Theme& theme, Color base, const ControlState& state) noexcept {
    if (state.disabled) return base;
    if (state.pressed) return blend_over(theme.colors.pressed_overlay, base);
    if (state.hovered) return blend_over(theme.colors.hover_overlay, base);
    return base;
}

// ----------------------------------------------------------------------------------------------------------------------
// Style resolvers
// ----------------------------------------------------------------------------------------------------------------------

BoxStyle button_style(const Theme& theme, ButtonVariant variant, const ControlState& state) noexcept {
    const ColorTokens& c = theme.colors;
    BoxStyle s;
    s.radii = CornerRadii::uniform(theme.radius.control);
    if (state.disabled) {
        // Every variant is disabled the same way: it stops asking for attention.
        s.fill = c.surface_raised;
        s.border = c.border_subtle;
        s.border_width = static_cast<float>(theme.size.border_width);
        s.content = c.text_disabled;
        return s;
    }
    switch (variant) {
    case ButtonVariant::Primary:
        // The accent has its own hover and pressed tokens (text_on_accent is contrast-checked against each).
        s.fill = state.pressed ? c.accent_pressed : (state.hovered ? c.accent_hover : c.accent);
        s.content = c.text_on_accent;
        break;
    case ButtonVariant::Destructive:
        // Pressed uses the danger_pressed token (as Primary uses accent_pressed): the pressed overlay would lighten
        // the fill in the dark theme and cost the label its contrast.
        s.fill = state.pressed ? c.danger_pressed : state_fill(theme, c.danger_solid, state);
        s.content = c.text_on_danger;
        break;
    case ButtonVariant::Default:
        s.fill = state_fill(theme, state.selected ? blend_over(c.accent_subtle, c.surface_raised) : c.surface_raised, state);
        s.border = c.border;
        s.border_width = static_cast<float>(theme.size.border_width);
        s.content = c.text_primary;
        break;
    }
    return s;
}

BoxStyle icon_button_style(const Theme& theme, const ControlState& state) noexcept {
    const ColorTokens& c = theme.colors;
    BoxStyle s;
    s.radii = CornerRadii::uniform(theme.radius.control);
    if (state.disabled) {
        s.content = c.text_disabled;
        return s;
    }
    s.fill = state_fill(theme, state.selected ? c.accent_subtle : kNone, state);
    s.content = state.selected ? c.accent : c.text_primary;
    return s;
}

BoxStyle panel_style(const Theme& theme, PanelVariant variant) noexcept {
    const ColorTokens& c = theme.colors;
    const float line = static_cast<float>(theme.size.border_width);
    BoxStyle s;
    s.content = c.text_primary;
    switch (variant) {
    case PanelVariant::Surface:
        s.fill = c.surface;
        break;
    case PanelVariant::Sunken:
        s.fill = c.surface_sunken;
        s.border = c.border_subtle;
        s.border_width = line;
        s.radii = CornerRadii::uniform(theme.radius.control);
        break;
    case PanelVariant::Raised:
        s.fill = c.surface_raised;
        s.border = c.border;
        s.border_width = line;
        s.radii = CornerRadii::uniform(theme.radius.md);
        break;
    case PanelVariant::Overlay:
        s.fill = c.surface_overlay;
        s.border = c.border;
        s.border_width = line;
        s.radii = CornerRadii::uniform(theme.radius.md);
        break;
    }
    return s;
}

BoxStyle list_row_style(const Theme& theme, const ControlState& state) noexcept {
    const ColorTokens& c = theme.colors;
    BoxStyle s;
    s.radii = CornerRadii::uniform(theme.radius.control);
    if (state.disabled) {
        s.content = c.text_disabled;
        return s;
    }
    s.fill = state_fill(theme, state.selected ? c.accent_subtle : kNone, state);
    s.content = c.text_primary;
    return s;
}

// ----------------------------------------------------------------------------------------------------------------------
// Geometry
// ----------------------------------------------------------------------------------------------------------------------

Rect focus_ring_bounds(const Theme& theme, const Rect& control) noexcept {
    if (control.width == 0 || control.height == 0) return {control.x, control.y, 0, 0};
    return grown(control, theme.size.focus_ring_offset + theme.size.focus_ring_width);
}

Rect selection_indicator_rect(const Theme& theme, const Rect& item, IndicatorEdge edge) noexcept {
    const int64_t thickness = std::max(theme.size.selection_indicator, 0);
    const int64_t inset = static_cast<int64_t>(std::max(theme.radius.control, 0.0f));
    const int64_t w = item.width;
    const int64_t h = item.height;
    if (thickness == 0) return {item.x, item.y, 0, 0};
    if (edge == IndicatorEdge::Leading) {
        if (w < thickness || h <= 2 * inset) return {item.x, item.y, 0, 0};
        return {item.x, clamp_i32(int64_t{item.y} + inset), clamp_u32(thickness), clamp_u32(h - 2 * inset)};
    }
    if (h < thickness || w <= 2 * inset) return {item.x, item.y, 0, 0};
    return {clamp_i32(int64_t{item.x} + inset), clamp_i32(int64_t{item.y} + h - thickness), clamp_u32(w - 2 * inset), clamp_u32(thickness)};
}

Rect separator_rect(const Theme& theme, const Rect& bounds, Orientation orientation) noexcept {
    const int64_t line = std::max(theme.size.border_width, 0);
    if (line == 0 || bounds.width == 0 || bounds.height == 0) return {bounds.x, bounds.y, 0, 0};
    if (orientation == Orientation::Horizontal) {
        const int64_t t = std::min<int64_t>(line, bounds.height);
        return {bounds.x, clamp_i32(int64_t{bounds.y} + (int64_t{bounds.height} - t) / 2), bounds.width, clamp_u32(t)};
    }
    const int64_t t = std::min<int64_t>(line, bounds.width);
    return {clamp_i32(int64_t{bounds.x} + (int64_t{bounds.width} - t) / 2), bounds.y, clamp_u32(t), bounds.height};
}

// ----------------------------------------------------------------------------------------------------------------------
// Painters
// ----------------------------------------------------------------------------------------------------------------------

void paint_box(Painter& painter, const Rect& rect, const BoxStyle& style) noexcept {
    painter.draw_box(rect, style.radii, style.fill, style.border_width, style.border);
}

void paint_focus_ring(Painter& painter, const Theme& theme, const Rect& control, const CornerRadii& radii) noexcept {
    const Rect ring = focus_ring_bounds(theme, control);
    if (ring.width == 0 || ring.height == 0) return;
    // Concentric with the control: each corner's radius grows by the distance to the ring's outer edge.
    const float grow = static_cast<float>(theme.size.focus_ring_offset + theme.size.focus_ring_width);
    const CornerRadii outer{radii.top_left + grow, radii.top_right + grow, radii.bottom_right + grow, radii.bottom_left + grow};
    painter.stroke_rounded_rect(ring, outer, static_cast<float>(theme.size.focus_ring_width), theme.colors.focus_ring);
}

void paint_button(Painter& painter, const Theme& theme, const TextRenderer* text, const Rect& rect, std::string_view label,
                  ButtonVariant variant, const ControlState& state) noexcept {
    const BoxStyle style = button_style(theme, variant, state);
    paint_box(painter, rect, style);
    draw_label(painter, text, rect, label, theme.type.body_strong, style.content, theme.spacing.md, theme.spacing.xs, true);
    if (state.focused && !state.disabled) paint_focus_ring(painter, theme, rect, style.radii);
}

void paint_icon_button(Painter& painter, const Theme& theme, const Rect& rect, IconId icon, const ControlState& state) noexcept {
    const BoxStyle style = icon_button_style(theme, state);
    paint_box(painter, rect, style);
    if (rect.width > 0 && rect.height > 0) {
        const Painter::ClipScope clip(painter, rect);
        if (clip.pushed()) draw_icon(painter, icon, rect, style.content);
    }
    if (state.focused && !state.disabled) paint_focus_ring(painter, theme, rect, style.radii);
}

void paint_panel(Painter& painter, const Theme& theme, const Rect& rect, PanelVariant variant) noexcept {
    paint_box(painter, rect, panel_style(theme, variant));
}

void paint_list_row(Painter& painter, const Theme& theme, const TextRenderer* text, const Rect& rect, std::string_view label,
                    const ControlState& state) noexcept {
    const BoxStyle style = list_row_style(theme, state);
    paint_box(painter, rect, style);
    if (state.selected) paint_selection_indicator(painter, theme, rect, IndicatorEdge::Leading, state);
    draw_label(painter, text, rect, label, theme.type.body, style.content, theme.spacing.sm, theme.spacing.xs, false);
    if (state.focused && !state.disabled) paint_focus_ring(painter, theme, rect, style.radii);
}

void paint_selection_indicator(Painter& painter, const Theme& theme, const Rect& item, IndicatorEdge edge,
                               const ControlState& state) noexcept {
    const Rect bar = selection_indicator_rect(theme, item, edge);
    if (bar.width == 0 || bar.height == 0) return;
    const float radius = static_cast<float>(theme.size.selection_indicator) * 0.5f; // a pill
    painter.fill_rounded_rect(bar, CornerRadii::uniform(radius), state.disabled ? theme.colors.text_disabled : theme.colors.accent);
}

void paint_separator(Painter& painter, const Theme& theme, const Rect& bounds, Orientation orientation) noexcept {
    painter.fill_rect(separator_rect(theme, bounds, orientation), theme.colors.border_subtle);
}

} // namespace vynexos::desktop
