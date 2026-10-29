#pragma once
#include "vynexos/desktop/window_decorator.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>

namespace vynexos::desktop {

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

    WindowMargins get_margins() const override;

private:
    std::shared_ptr<IWidgetToolkit> m_toolkit;
};

} // namespace vynexos::desktop
