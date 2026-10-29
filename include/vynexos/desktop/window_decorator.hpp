#pragma once
#include <memory>
#include <string_view>
#include <cstdint>
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

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
};

} // namespace vynexos::desktop
