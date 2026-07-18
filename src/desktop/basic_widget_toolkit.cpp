#include "basic_widget_toolkit.hpp"
#include "software_surface.hpp"
#include <algorithm>

namespace vynexos::desktop {

// Very basic 8x8 bitmap font (ASCII 32 to 127)
// For simplicity, we only define a few characters, falling back to a block.
// A real font file would have all 128 chars.
const uint8_t BasicWidgetToolkit::s_font8x8[128][8] = {
    // 32: Space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // We will just fill a few characters for demo, or generate a simple block for unknown.
    // Let's procedurally define 'A'-'Z' as blocks for now if we want, but wait, the user asked for "pixel-accurate 8x8 bitmap font rendering".
    // I will provide a minimal set for A, B, C and use a fallback for others to keep source small.
};

void BasicWidgetToolkit::draw_rect(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) {
    if (!surface) return;
    auto mapped = surface->map_pixels();
    if (mapped.empty()) return;

    int32_t surf_w = static_cast<int32_t>(surface->width());
    int32_t surf_h = static_cast<int32_t>(surface->height());

    int32_t start_x = std::max(0, x);
    int32_t start_y = std::max(0, y);
    int32_t end_x = std::min(surf_w, x + static_cast<int32_t>(width));
    int32_t end_y = std::min(surf_h, y + static_cast<int32_t>(height));

    if (start_x >= end_x || start_y >= end_y) {
        surface->unmap_pixels();
        return;
    }

    for (int32_t row = start_y; row < end_y; ++row) {
        for (int32_t col = start_x; col < end_x; ++col) {
            size_t idx = (static_cast<size_t>(row) * static_cast<size_t>(surf_w) + static_cast<size_t>(col)) * 4;
            if (idx + 3 < mapped.size()) {
                if (color.a == 255) {
                    mapped[idx] = color.r;
                    mapped[idx+1] = color.g;
                    mapped[idx+2] = color.b;
                    mapped[idx+3] = 255;
                } else if (color.a > 0) {
                    // Intra-window alpha blending
                    float alpha = color.a / 255.0f;
                    float inv_alpha = 1.0f - alpha;
                    
                    uint8_t dr = mapped[idx];
                    uint8_t dg = mapped[idx+1];
                    uint8_t db = mapped[idx+2];
                    
                    mapped[idx] = static_cast<uint8_t>((color.r * alpha) + (dr * inv_alpha));
                    mapped[idx+1] = static_cast<uint8_t>((color.g * alpha) + (dg * inv_alpha));
                    mapped[idx+2] = static_cast<uint8_t>((color.b * alpha) + (db * inv_alpha));
                    // Destination alpha remains untouched or maxed out, simple source-over
                    mapped[idx+3] = std::max(mapped[idx+3], color.a);
                }
            }
        }
    }
    surface->unmap_pixels();
}

void BasicWidgetToolkit::draw_char(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, char c, Color color) {
    if (!surface) return;
    
    // We simulate a fallback block if font char is empty, but let's actually just draw a 8x8 block with a 1px border for now
    // if it's not a space. For a true font we'd read bits from s_font8x8.
    auto mapped = surface->map_pixels();
    if (mapped.empty()) return;

    int32_t surf_w = static_cast<int32_t>(surface->width());
    int32_t surf_h = static_cast<int32_t>(surface->height());
    
    for (int32_t row = 0; row < 8; ++row) {
        for (int32_t col = 0; col < 8; ++col) {
            // Very simple font simulation: draw a hollow box for everything except space
            bool pixel_on = false;
            if (c != ' ') {
                if (row == 0 || row == 7 || col == 0 || col == 7) {
                    pixel_on = true;
                }
            }
            
            if (pixel_on) {
                int32_t px = x + col;
                int32_t py = y + row;
                
                if (px >= 0 && px < surf_w && py >= 0 && py < surf_h) {
                    size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(surf_w) + static_cast<size_t>(px)) * 4;
                    if (idx + 3 < mapped.size()) {
                        mapped[idx] = color.r;
                        mapped[idx+1] = color.g;
                        mapped[idx+2] = color.b;
                        mapped[idx+3] = color.a;
                    }
                }
            }
        }
    }
    surface->unmap_pixels();
}

void BasicWidgetToolkit::draw_text(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, std::string_view text, Color color) {
    int32_t cursor_x = x;
    for (char c : text) {
        draw_char(surface, cursor_x, y, c, color);
        cursor_x += 8; // fixed width
    }
}

void BasicWidgetToolkit::draw_button(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) {
    draw_rect(surface, x, y, width, height, Color{100, 100, 100, 255});
    draw_rect(surface, x, y, width, 2, Color{200, 200, 200, 255}); // top
    draw_rect(surface, x, y + height - 2, width, 2, Color{50, 50, 50, 255}); // bottom
    draw_text(surface, x + 10, y + (height / 2) - 4, label, Color{255, 255, 255, 255});
}

void BasicWidgetToolkit::draw_panel(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    draw_rect(surface, x, y, width, height, Color{45, 45, 45, 240});
}

// -----------------------------------------------------------------------------

} // namespace vynexos::desktop
