#include <iostream>
#include <cassert>
#include <memory>
#include "vynexos/desktop/window_manager.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../desktop/software_surface.hpp"

using namespace vynexos::desktop;
using namespace vynexos;

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel level) override { (void)level; }
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel level, std::string_view message) override { (void)level; (void)message; }
};

void test_scene_compilation() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    BasicWindowManager wm(event_bus, logger);
    
    // Create two windows
    uint32_t w1 = wm.create_window("Window 1", {10, 10, 100, 100});
    uint32_t w2 = wm.create_window("Window 2", {20, 20, 200, 200});
    
    // They don't have surfaces yet, so scene should be empty
    auto scene1 = wm.build_scene();
    assert(scene1.empty());
    
    // Submit native surfaces
    auto surf1 = std::make_shared<vynexos::desktop::SoftwareSurface>(100, 100);
    wm.set_window_surface(w1, surf1);
    
    auto surf2 = std::make_shared<vynexos::desktop::SoftwareSurface>(200, 200);
    wm.set_window_surface(w2, surf2);
    
    auto scene2 = wm.build_scene();
    assert(scene2.size() == 2);
    
    // Z-order: bottom to top
    // create_window pushes to front (Wait, check implementation: basic_window_manager.cpp pushes back)
    // The implementation pushes back by default.
    // So w1 is at index 0, w2 is at index 1.
    assert(scene2[0].bounds.width == 100);
    assert(scene2[1].bounds.width == 200);
    
    // Focus w1, which moves it to the top (back of z_order)
    wm.focus_window(w1);
    
    auto scene3 = wm.build_scene();
    assert(scene3.size() == 2);
    assert(scene3[0].bounds.width == 200);
    assert(scene3[1].bounds.width == 100);
    
    std::cout << "test_scene_compilation passed\n";
}

int main() {
    std::cout << "Running Window Manager Orchestration Tests...\n";
    test_scene_compilation();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
