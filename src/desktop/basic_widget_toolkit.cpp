#include "basic_widget_toolkit.hpp"
#include "software_surface.hpp"
#include "font8x8_basic.h"
#include <algorithm>

namespace vynexos::desktop {


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

    uint8_t* mapped_ptr = mapped.data();

    for (int32_t row = start_y; row < end_y; ++row) {
        size_t idx = (static_cast<size_t>(row) * static_cast<size_t>(surf_w) + static_cast<size_t>(start_x)) * 4;
        for (int32_t col = start_x; col < end_x; ++col) {
            if (color.a == 255) {
                mapped_ptr[idx] = color.r;
                mapped_ptr[idx+1] = color.g;
                mapped_ptr[idx+2] = color.b;
                mapped_ptr[idx+3] = 255;
            } else if (color.a > 0) {
                float alpha = color.a / 255.0f;
                float inv_alpha = 1.0f - alpha;
                
                uint8_t dr = mapped_ptr[idx];
                uint8_t dg = mapped_ptr[idx+1];
                uint8_t db = mapped_ptr[idx+2];
                
                mapped_ptr[idx] = static_cast<uint8_t>((color.r * alpha) + (dr * inv_alpha));
                mapped_ptr[idx+1] = static_cast<uint8_t>((color.g * alpha) + (dg * inv_alpha));
                mapped_ptr[idx+2] = static_cast<uint8_t>((color.b * alpha) + (db * inv_alpha));
                mapped_ptr[idx+3] = std::max(mapped_ptr[idx+3], color.a);
            }
            idx += 4;
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
    
    uint8_t* mapped_ptr = mapped.data();
    for (int32_t row = 0; row < 8; ++row) {
        for (int32_t col = 0; col < 8; ++col) {
            bool pixel_on = false;
            if (c >= 0 && c < 128) {
                if (font8x8_basic[static_cast<uint8_t>(c)][row] & (1 << col)) {
                    pixel_on = true;
                }
            }
            
            if (pixel_on) {
                int32_t px = x + col;
                int32_t py = y + row;
                
                if (px >= 0 && px < surf_w && py >= 0 && py < surf_h) {
                    size_t idx = (static_cast<size_t>(py) * static_cast<size_t>(surf_w) + static_cast<size_t>(px)) * 4;
                    mapped_ptr[idx] = color.r;
                    mapped_ptr[idx+1] = color.g;
                    mapped_ptr[idx+2] = color.b;
                    mapped_ptr[idx+3] = color.a;
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
    draw_rect(surface, x, y, width, height, Color{45, 45, 45, 255}); // Opaque to prevent click confusion
}

// -----------------------------------------------------------------------------

} // namespace vynexos::desktop
