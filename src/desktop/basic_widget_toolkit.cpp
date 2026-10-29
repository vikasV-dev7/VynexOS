#include "basic_widget_toolkit.hpp"
#include "software_surface.hpp"
#include "vynexos/desktop/painter.hpp"
#include "font8x8_basic.h"
#include <algorithm>

namespace vynexos::desktop {


void BasicWidgetToolkit::draw_rect(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) {
    if (!surface) return;
    Painter painter(*surface);
    painter.fill_rect(Rect{x, y, width, height}, color);
}

void BasicWidgetToolkit::draw_rounded_rect(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii, Color color) {
    if (!surface) return;
    Painter painter(*surface);
    painter.fill_rounded_rect(rect, radii, color);
}

void BasicWidgetToolkit::draw_rounded_border(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii, float thickness,
                                             Color color) {
    if (!surface) return;
    Painter painter(*surface);
    painter.stroke_rounded_rect(rect, radii, thickness, color);
}

void BasicWidgetToolkit::draw_gradient_rect(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii,
                                            const LinearGradient& gradient) {
    if (!surface) return;
    Painter painter(*surface);
    painter.fill_rounded_rect(rect, radii, gradient);
}

void BasicWidgetToolkit::draw_shadow(std::shared_ptr<ISurface> surface, const Rect& box, const CornerRadii& radii, const ShadowSpec& shadow) {
    if (!surface) return;
    Painter painter(*surface);
    painter.draw_shadow(box, radii, shadow);
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
            if (static_cast<uint8_t>(c) < 128) {
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
    draw_rect(surface, x, y + static_cast<int32_t>(height) - 2, width, 2, Color{50, 50, 50, 255}); // bottom
    draw_text(surface, x + 10, y + static_cast<int32_t>(height / 2) - 4, label, Color{255, 255, 255, 255});
}

void BasicWidgetToolkit::draw_panel(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    draw_rect(surface, x, y, width, height, Color{45, 45, 45, 255}); // Opaque to prevent click confusion
}

// -----------------------------------------------------------------------------

} // namespace vynexos::desktop
