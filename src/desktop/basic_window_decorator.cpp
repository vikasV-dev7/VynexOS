#include "basic_window_decorator.hpp"
#include "vynexos/desktop/icons.hpp"
#include "vynexos/desktop/painter.hpp"
#include <algorithm>

namespace vynexos::desktop {

namespace {

/// Every colour the chrome uses, resolved once per draw from the tokens and the focus state.
struct ChromePalette {
    Color body;
    Color title_bg;
    Color border;
    Color separator;
    Color title_text;
    Color icon;
};

ChromePalette make_palette(const Theme& theme, bool focused) {
    const ColorTokens& c = theme.colors;
    ChromePalette p;
    p.body = c.surface;
    p.separator = c.border_subtle;
    if (focused) {
        p.title_bg = c.surface_raised;
        p.border = mix(c.border, c.accent, 0.4f);
        p.title_text = c.text_primary;
        p.icon = c.text_primary;
    } else {
        p.title_bg = c.surface;
        p.border = c.border_subtle;
        p.title_text = c.text_secondary;
        p.icon = c.text_secondary;
    }
    return p;
}

Rect absolute(const Rect& r) { return r; }

void draw_control(Painter& painter, const Theme& theme, const ChromePalette& palette, IconId icon, const Rect& button,
                  bool hovered, bool pressed, bool is_close) {
    if (button.width == 0 || button.height == 0) return;
    const ColorTokens& c = theme.colors;
    Color icon_color = palette.icon;
    if (is_close && (hovered || pressed)) {
        painter.fill_rounded_rect(button, CornerRadii::uniform(theme.radius.sm), pressed ? c.danger_pressed : c.danger);
        icon_color = c.text_primary;
    } else if (pressed) {
        painter.fill_rounded_rect(button, CornerRadii::uniform(theme.radius.sm), c.pressed_overlay);
    } else if (hovered) {
        painter.fill_rounded_rect(button, CornerRadii::uniform(theme.radius.sm), c.hover_overlay);
    }
    draw_icon(painter, icon, button, icon_color);
}

} // namespace

BasicWindowDecorator::BasicWindowDecorator(std::shared_ptr<IWidgetToolkit> toolkit)
    : m_toolkit(std::move(toolkit)) {}

void BasicWindowDecorator::draw_decorations(
    std::shared_ptr<ISurface> surface,
    const DecorationLayout& layout,
    std::string_view title,
    bool focused,
    WindowState state,
    const WindowTheme& theme) {
    DecorationVisualState visual;
    visual.focused = focused;
    visual.state = state;
    draw_decorations(std::move(surface), layout, title, visual, theme);
}

void BasicWindowDecorator::draw_decorations(
    std::shared_ptr<ISurface> surface,
    const DecorationLayout& layout,
    std::string_view title,
    const DecorationVisualState& visual,
    const WindowTheme& /*legacy_theme*/) {
    if (!surface || surface->width() == 0 || surface->height() == 0) return;

    {
        auto map = surface->map_pixels();
        std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
        surface->unmap_pixels();
    }

    const Theme& theme = m_toolkit->theme();
    const ChromePalette palette = make_palette(theme, visual.focused);
    const bool maximized = (visual.state == WindowState::Maximized);
    const Rect frame = absolute(layout.visible_frame);
    if (frame.width == 0 || frame.height == 0) return;

    // Maximized windows sit flush with the screen: no rounding, no border. Bottom corners stay square until the
    // corner-mask stage, because the client surface is drawn on top and is rectangular.
    const float radius = maximized ? 0.0f : theme.radius.window;
    Painter painter(*surface);

    painter.fill_rounded_rect(frame, CornerRadii::top(radius), palette.body);

    const Rect title_bar = layout.title_bar;
    if (title_bar.width > 0 && title_bar.height > 0) {
        painter.fill_rounded_rect(title_bar, CornerRadii::top(radius), palette.title_bg);
        painter.fill_rect({title_bar.x, title_bar.y + static_cast<int32_t>(title_bar.height) - 1, title_bar.width, 1}, palette.separator);
    }

    if (!maximized) {
        painter.stroke_rounded_rect(frame, CornerRadii::top(radius), static_cast<float>(theme.size.window_border), palette.border);
    }

    // Controls.
    const bool close_hot = visual.hovered == DecorationHitRegion::CloseButton;
    const bool min_hot = visual.hovered == DecorationHitRegion::MinimizeButton;
    const bool max_hot = visual.hovered == DecorationHitRegion::MaximizeButton;
    draw_control(painter, theme, palette, IconId::Close, layout.close_button, close_hot, visual.pressed == DecorationHitRegion::CloseButton, true);
    draw_control(painter, theme, palette, maximized ? IconId::Restore : IconId::Maximize, layout.maximize_button, max_hot,
                 visual.pressed == DecorationHitRegion::MaximizeButton, false);
    draw_control(painter, theme, palette, IconId::Minimize, layout.minimize_button, min_hot,
                 visual.pressed == DecorationHitRegion::MinimizeButton, false);

    // Title: left aligned, vertically centred on the cap height, truncated before the first control.
    if (!title.empty() && title_bar.width > 0 && title_bar.height > 0) {
        const TextStyle& style = theme.type.body_strong;
        const FontMetrics fm = m_toolkit->font_metrics(style);
        const int32_t text_x = title_bar.x + theme.spacing.md;
        int32_t right_limit = title_bar.x + static_cast<int32_t>(title_bar.width) - theme.spacing.md;
        for (const Rect* b : {&layout.minimize_button, &layout.maximize_button, &layout.close_button}) {
            if (b->width > 0) right_limit = std::min(right_limit, b->x - theme.spacing.sm);
        }
        const float max_width = static_cast<float>(right_limit - text_x);
        if (max_width > 0.0f) {
            const int32_t baseline = title_bar.y + static_cast<int32_t>((static_cast<float>(title_bar.height) + fm.cap_height) * 0.5f);
            m_toolkit->draw_text_ellipsized(surface, text_x, baseline, title, style, palette.title_text, max_width);
        }
    }
}

WindowMargins BasicWindowDecorator::get_margins() const {
    return frame_style(WindowState::Normal).margins;
}

FrameStyle BasicWindowDecorator::frame_style(WindowState state) const {
    const Theme& theme = m_toolkit->theme();
    const int32_t grip = theme.size.resize_grip;
    const int32_t border = theme.size.window_border;
    const int32_t title = theme.size.titlebar_height;

    FrameStyle style;
    style.button_size = static_cast<uint32_t>(theme.size.control_button);
    style.button_spacing = static_cast<uint32_t>(theme.size.control_button_gap);
    style.button_margin_top = static_cast<uint32_t>((title - theme.size.control_button) / 2);
    style.button_margin_right = static_cast<uint32_t>(theme.spacing.sm);

    if (state == WindowState::Maximized) {
        // Flush with the work area: no invisible band, no border.
        style.margins = WindowMargins{title, 0, 0, 0};
        style.border_thickness = 0;
        style.resize_inset = 0;
    } else {
        style.margins = WindowMargins{grip + title, grip + border, grip + border, grip + border};
        style.border_thickness = static_cast<uint32_t>(border);
        style.resize_inset = static_cast<uint32_t>(grip);
    }
    return style;
}

} // namespace vynexos::desktop
