#include <iostream>
#include <cassert>
#include <memory>
#include "vynexos/desktop/compositor.hpp"
#include "../desktop/basic_compositor.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/display_abstraction.hpp"

using namespace vynexos::desktop;
using namespace vynexos;

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel level) override { (void)level; }
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel level, std::string_view message) override { (void)level; (void)message; }
};

class TestDisplayBackend : public IDisplayAbstraction {
public:
    hal::FrameBuffer last_frame;

    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last_frame = buffer; // Copy the frame buffer
        return {};
    }
    void shutdown() override {}
};

void test_software_surface() {
    auto logger = std::make_shared<TestLogger>();
    auto display = std::make_shared<TestDisplayBackend>();
    BasicCompositor compositor(display, logger);

    auto surface = compositor.create_surface(100, 100);
    assert(surface->width() == 100);
    assert(surface->height() == 100);

    auto pixels = surface->map_pixels();
    assert(pixels.size() == 100 * 100 * 4);
    
    // Write something
    pixels[0] = 255;
    pixels[1] = 0;
    pixels[2] = 0;
    pixels[3] = 255;
    
    surface->unmap_pixels();
    
    std::cout << "test_software_surface passed\n";
}

void test_alpha_blending() {
    auto logger = std::make_shared<TestLogger>();
    auto display = std::make_shared<TestDisplayBackend>();
    BasicCompositor compositor(display, logger);

    auto surface_bg = compositor.create_surface(100, 100);
    auto p_bg = surface_bg->map_pixels();
    for (size_t i = 0; i < p_bg.size(); i += 4) {
        p_bg[i] = 255;   // R
        p_bg[i+1] = 0;   // G
        p_bg[i+2] = 0;   // B
        p_bg[i+3] = 255; // A
    }
    surface_bg->unmap_pixels();

    auto surface_fg = compositor.create_surface(100, 100);
    auto p_fg = surface_fg->map_pixels();
    for (size_t i = 0; i < p_fg.size(); i += 4) {
        p_fg[i] = 0;     // R
        p_fg[i+1] = 255; // G
        p_fg[i+2] = 0;   // B
        p_fg[i+3] = 127; // A (~50% transparent)
    }
    surface_fg->unmap_pixels();

    SceneGraph scene;
    scene.push_back({ {0, 0, 100, 100}, {0, 0, 1920, 1080}, 1.0f, true, surface_bg });
    scene.push_back({ {0, 0, 100, 100}, {0, 0, 1920, 1080}, 1.0f, true, surface_fg });

    auto res = compositor.render_frame(scene);
    assert(res.has_value());

    const auto& frame = display->last_frame;
    assert(frame.width == 1920);
    assert(frame.height == 1080);
    
    // Pixel at 0,0 should be ~50% red, ~50% green
    uint8_t r = frame.pixels[0];
    uint8_t g = frame.pixels[1];
    uint8_t b = frame.pixels[2];
    uint8_t a = frame.pixels[3];
    
    assert(r > 0 && r < 255);
    assert(g > 0 && g < 255);
    assert(b == 0);
    assert(a == 255);
    
    std::cout << "test_alpha_blending passed\n";
}

int main() {
    std::cout << "Running Compositor Tests...\n";
    test_software_surface();
    test_alpha_blending();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
