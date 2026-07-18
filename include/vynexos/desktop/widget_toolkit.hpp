#pragma once
#include <string_view>
#include <vector>
#include <cstdint>

#include "vynexos/desktop/desktop_types.hpp"
#include <memory>

namespace vynexos::desktop {

struct Color {
    uint8_t r, g, b, a;
};

class IWidgetToolkit {
public:
    virtual ~IWidgetToolkit() = default;

    // New Native Surface Interface
    virtual void draw_rect(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) = 0;
    virtual void draw_text(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, std::string_view text, Color color) = 0;
    virtual void draw_button(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) = 0;
    virtual void draw_panel(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;


};

} // namespace vynexos::desktop
