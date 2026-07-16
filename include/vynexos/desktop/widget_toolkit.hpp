#pragma once
#include <string_view>
#include <vector>
#include <cstdint>
#include "vynexos/desktop/compositor.hpp" // For WindowBuffer

namespace vynexos::desktop {

struct Color {
    uint8_t r, g, b, a;
};

class IWidgetToolkit {
public:
    virtual ~IWidgetToolkit() = default;

    // Draw operations directly manipulate the pixel vector of a given WindowBuffer
    virtual void draw_rect(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) = 0;
    virtual void draw_text(WindowBuffer& buffer, int32_t x, int32_t y, std::string_view text, Color color) = 0;
    
    // High-level native OS widgets
    virtual void draw_button(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) = 0;
    virtual void draw_panel(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;
};

} // namespace vynexos::desktop
