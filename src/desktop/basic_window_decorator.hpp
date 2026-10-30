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

    WindowMargins get_margins() const override;
    [[nodiscard]] FrameStyle frame_style(WindowState state) const override;

private:
    std::shared_ptr<IWidgetToolkit> m_toolkit;
};

} // namespace vynexos::desktop
