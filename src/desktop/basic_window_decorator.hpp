#pragma once
#include "vynexos/desktop/window_decorator.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>

namespace vynexos::desktop {

/// Draws window chrome from the toolkit's design tokens. The WindowTheme argument of the legacy interface is accepted
/// for compatibility but ignored: colours come from the token theme so every window looks the same.
class BasicWindowDecorator final : public IWindowDecorator {
public:
    explicit BasicWindowDecorator(std::shared_ptr<IWidgetToolkit> toolkit);
    ~BasicWindowDecorator() override = default;

    void draw_decorations(
        std::shared_ptr<ISurface> surface,
        const DecorationLayout& layout,
        std::string_view title,
        bool focused,
        WindowState state,
        const WindowTheme& theme) override;

    void draw_decorations(
        std::shared_ptr<ISurface> surface,
        const DecorationLayout& layout,
        std::string_view title,
        const DecorationVisualState& visual,
        const WindowTheme& theme) override;

    /// Exact for any region that does not touch the title text (e.g. a control button); refuses regions that do.
    bool redraw_decoration_region(
        std::shared_ptr<ISurface> surface,
        const DecorationLayout& layout,
        std::string_view title,
        const DecorationVisualState& visual,
        const WindowTheme& theme,
        const Rect& region) override;

    WindowMargins get_margins() const override;
    [[nodiscard]] FrameStyle frame_style(WindowState state) const override;

private:
    std::shared_ptr<IWidgetToolkit> m_toolkit;

    /// Draws the frame and controls (everything except the title text), limited to `clip`.
    void paint_frame(ISurface& surface, const DecorationLayout& layout, const DecorationVisualState& visual, const Rect& clip) const;
    /// Where the title text may be drawn (empty when there is no room).
    [[nodiscard]] Rect title_text_box(const DecorationLayout& layout) const;
};

} // namespace vynexos::desktop
