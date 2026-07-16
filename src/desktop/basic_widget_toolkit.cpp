#include "basic_widget_toolkit.hpp"
#include <algorithm>

namespace vynexos::desktop {

void BasicWidgetToolkit::draw_rect(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) {
    if (buffer.pixels.empty()) return;
    
    // Basic bounds checking
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + width > buffer.width) width = buffer.width - x;
    if (y + height > buffer.height) height = buffer.height - y;
    
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            size_t idx = ((y + row) * buffer.width + (x + col)) * 4;
            if (idx + 3 < buffer.pixels.size()) {
                buffer.pixels[idx] = color.r;
                buffer.pixels[idx+1] = color.g;
                buffer.pixels[idx+2] = color.b;
                buffer.pixels[idx+3] = color.a;
            }
        }
    }
}

void BasicWidgetToolkit::draw_text(WindowBuffer& buffer, int32_t x, int32_t y, std::string_view text, Color color) {
    // In a real toolkit, this invokes FreeType or HarfBuzz.
    // For the basic mock toolkit, we simulate text by drawing a thin bounding box to represent it.
    draw_rect(buffer, x, y, static_cast<uint32_t>(text.size() * 8), 12, color);
}

void BasicWidgetToolkit::draw_button(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) {
    // Button background
    draw_rect(buffer, x, y, width, height, Color{100, 100, 100, 255});
    // Button border
    draw_rect(buffer, x, y, width, 2, Color{200, 200, 200, 255}); // top
    draw_rect(buffer, x, y + height - 2, width, 2, Color{50, 50, 50, 255}); // bottom
    // Button text (simulated)
    draw_text(buffer, x + 10, y + (height / 2) - 6, label, Color{255, 255, 255, 255});
}

void BasicWidgetToolkit::draw_panel(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    // Draw a generic OS panel background
    draw_rect(buffer, x, y, width, height, Color{45, 45, 45, 240});
}

} // namespace vynexos::desktop
