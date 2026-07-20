#include <iostream>
#include <cassert>
#include <memory>
#include "vynexos/desktop/widget_toolkit.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/software_surface.hpp"
#include "../desktop/software_surface.hpp"

using namespace vynexos::desktop;

void test_rect_drawing() {
    BasicWidgetToolkit toolkit;
    auto surface = std::make_shared<SoftwareSurface>(100, 100);
    
    // Test rect
    toolkit.draw_rect(surface, 10, 10, 50, 50, Color{255, 0, 0, 255});
    
    auto map = surface->map_pixels();
    // Verify pixel at 10,10 is red
    size_t idx = (10 * 100 + 10) * 4;
    assert(map[idx] == 255);
    assert(map[idx+1] == 0);
    assert(map[idx+2] == 0);
    assert(map[idx+3] == 255);
    
    // Verify pixel at 9,9 is empty (0)
    size_t idx_out = (9 * 100 + 9) * 4;
    assert(map[idx_out] == 0);
    surface->unmap_pixels();
    
    std::cout << "test_rect_drawing passed\n";
}

void test_alpha_rect_drawing() {
    BasicWidgetToolkit toolkit;
    auto surface = std::make_shared<SoftwareSurface>(100, 100);
    
    // Background is white
    toolkit.draw_rect(surface, 0, 0, 100, 100, Color{255, 255, 255, 255});
    
    // Draw 50% transparent red
    toolkit.draw_rect(surface, 10, 10, 10, 10, Color{255, 0, 0, 127});
    
    auto mapped = surface->map_pixels();
    size_t idx = (10 * 100 + 10) * 4;
    
    uint8_t r = mapped[idx];
    uint8_t g = mapped[idx+1];
    uint8_t b = mapped[idx+2];
    
    assert(r == 255);
    assert(g > 0 && g < 255);
    assert(b > 0 && b < 255);
    
    surface->unmap_pixels();
    std::cout << "test_alpha_rect_drawing passed\n";
}

void test_panel_drawing() {
    BasicWidgetToolkit toolkit;
    
    auto surface = std::make_shared<SoftwareSurface>(100, 100);
    
    toolkit.draw_panel(surface, 10, 10, 10, 10);
    
    auto mapped = surface->map_pixels();
    size_t idx = (10 * 100 + 10) * 4;
    // {45, 45, 45, 255} over {0,0,0,0} = {45, 45, 45, 255}
    assert(mapped[idx] == 45);
    assert(mapped[idx+1] == 45);
    assert(mapped[idx+2] == 45);
    assert(mapped[idx+3] == 255);
    surface->unmap_pixels();
    
    std::cout << "test_panel_drawing passed\n";
}

int main() {
    std::cout << "Running Widget Toolkit Tests...\n";
    test_rect_drawing();
    test_alpha_rect_drawing();
    test_panel_drawing();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
