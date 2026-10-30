#pragma once
#include <memory>
#include <string_view>
#include <cstdint>
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

/// What a decoration looks like right now.
struct DecorationVisualState {
    bool focused{false};
    WindowState state{WindowState::Normal};
    DecorationHitRegion hovered{DecorationHitRegion::None};  // control under the pointer, if any
    DecorationHitRegion pressed{DecorationHitRegion::None};  // control being pressed, if any
};

class IWindowDecorator {
public:
    virtual ~IWindowDecorator() = default;

    // Decorates the provided surface according to the window layout and theme.
    virtual void draw_decorations(
        std::shared_ptr<ISurface> surface, 
        const DecorationLayout& layout,
        std::string_view title, 
        bool focused,
        WindowState state,
        const WindowTheme& theme) = 0;

    // Returns the standard margins required for the decorations
    virtual WindowMargins get_margins() const = 0;

    // Draws with interaction state (hover/pressed control). Decorators without interaction visuals fall back to the
    // plain call above.
    virtual void draw_decorations(
        std::shared_ptr<ISurface> surface,
        const DecorationLayout& layout,
        std::string_view title,
        const DecorationVisualState& visual,
        const WindowTheme& theme) {
        draw_decorations(std::move(surface), layout, title, visual.focused, visual.state, theme);
    }

    // Repaints only `region` (surface coordinates) of an existing decoration so it reflects `visual`, leaving every
    // other pixel untouched. Inside the region the result equals a full draw_decorations() with the same arguments.
    // Returns false when the decorator cannot update that region on its own; the caller then redraws everything.
    virtual bool redraw_decoration_region(
        std::shared_ptr<ISurface> surface,
        const DecorationLayout& layout,
        std::string_view title,
        const DecorationVisualState& visual,
        const WindowTheme& theme,
        const Rect& region) {
        (void)surface;
        (void)layout;
        (void)title;
        (void)visual;
        (void)theme;
        (void)region;
        return false;
    }

    // The frame geometry this decorator wants for a window in the given state. The default derives it from
    // get_margins() with no invisible resize band, i.e. the original frame.
    [[nodiscard]] virtual FrameStyle frame_style(WindowState state) const {
        (void)state;
        FrameStyle style;
        style.margins = get_margins();
        return style;
    }
};

} // namespace vynexos::desktop
