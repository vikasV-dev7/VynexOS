#include <iostream>
#include "test_check.hpp"
#include <memory>
#include <algorithm>
#include <vector>
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
    CHECK(scene1.empty());
    
    // Submit native surfaces
    auto surf1 = std::make_shared<vynexos::desktop::SoftwareSurface>(100, 100);
    wm.set_window_surface(w1, surf1);
    
    auto surf2 = std::make_shared<vynexos::desktop::SoftwareSurface>(200, 200);
    wm.set_window_surface(w2, surf2);
    
    auto scene2 = wm.build_scene();
    CHECK(scene2.size() == 2);
    
    // Z-order: bottom to top
    // create_window pushes to front (Wait, check implementation: basic_window_manager.cpp pushes back)
    // The implementation pushes back by default.
    // So w1 is at index 0, w2 is at index 1.
    CHECK(scene2[0].bounds.width == 100);
    CHECK(scene2[1].bounds.width == 200);
    
    // Focus w1, which moves it to the top (back of z_order)
    wm.focus_window(w1);
    
    auto scene3 = wm.build_scene();
    CHECK(scene3.size() == 2);
    CHECK(scene3[0].bounds.width == 200);
    CHECK(scene3[1].bounds.width == 100);
    
    std::cout << "test_scene_compilation passed\n";
}

void test_stress_focus_transitions() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    BasicWindowManager wm(event_bus, logger);
    std::vector<uint32_t> window_ids;
    
    // Create 10 overlapping windows
    for (int i = 0; i < 10; i++) {
        uint32_t id = wm.create_window("Win" + std::to_string(i), {i*10, i*10, 100, 100});
        auto surf = std::make_shared<vynexos::desktop::SoftwareSurface>(100, 100);
        wm.set_window_surface(id, surf);
        window_ids.push_back(id);
    }
    
    CHECK(wm.get_windows_z_ordered().size() == 10);
    
    // Simulate rapid, random focus events
    for (int i = 0; i < 10000; i++) {
        uint32_t target = window_ids[static_cast<size_t>(i % 10)];
        wm.focus_window(target);
    }
    
    auto final_z_order = wm.get_windows_z_ordered();
    
    // Validate exactly one of each window exists in the Z-order
    CHECK(final_z_order.size() == 10);
    for (uint32_t id : window_ids) {
        CHECK(std::count(final_z_order.begin(), final_z_order.end(), id) == 1);
    }
    
    // Check missing windows
    auto scene = wm.build_scene();
    CHECK(scene.size() == 10);
    
    std::cout << "test_stress_focus_transitions passed\n";
}

void test_resize_constraints() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    BasicWindowManager wm(event_bus, logger);
    
    uint32_t w1 = wm.create_window("Window 1", {10, 10, 200, 200});
    
    // Attempt to resize below min dimensions (default min is 100x50)
    wm.resize_window(w1, 50, 20);
    
    auto geom = wm.get_window_geometry(w1);
    CHECK(geom.has_value());
    CHECK(geom->width == 100);
    CHECK(geom->height == 50);
    
    // Attempt set_window_geometry
    wm.set_window_geometry(w1, {20, 20, 10, 10});
    geom = wm.get_window_geometry(w1);
    CHECK(geom->x == 20);
    CHECK(geom->y == 20);
    CHECK(geom->width == 100);
    CHECK(geom->height == 50);
    
    std::cout << "test_resize_constraints passed\n";
}

void test_resize_hit_testing() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    BasicWindowManager wm(event_bus, logger);
    
    uint32_t w1 = wm.create_window("Window 1", {10, 10, 200, 200});
    
    // Test Top Left
    auto hit1 = wm.get_resize_handle_at(10, 10);
    CHECK(hit1.has_value() && hit1->window_id == w1 && hit1->edge == ResizeEdge::TopLeft);
    
    // Test Bottom Right
    auto hit2 = wm.get_resize_handle_at(205, 205);
    CHECK(hit2.has_value() && hit2->window_id == w1 && hit2->edge == ResizeEdge::BottomRight);
    
    // Test interior
    auto hit3 = wm.get_resize_handle_at(100, 100);
    CHECK(!hit3.has_value());
    
    // Test Right
    auto hit4 = wm.get_resize_handle_at(205, 100);
    CHECK(hit4.has_value() && hit4->window_id == w1 && hit4->edge == ResizeEdge::Right);
    
    std::cout << "test_resize_hit_testing passed\n";
}

void test_taskbar_enumeration() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    BasicWindowManager wm(event_bus, logger);
    
    uint32_t w1 = wm.create_window("Window 1", {10, 10, 100, 100});
    uint32_t w2 = wm.create_window("Window 2", {20, 20, 200, 200});
    uint32_t w3 = wm.create_window("Window 3", {30, 30, 300, 300});
    
    // Check enumeration order is 1, 2, 3
    auto snaps = wm.get_window_snapshots();
    CHECK(snaps.size() == 3);
    CHECK(snaps[0].window_id == w1);
    CHECK(snaps[1].window_id == w2);
    CHECK(snaps[2].window_id == w3);
    
    // Focus w1, which changes Z-order
    wm.focus_window(w1);
    CHECK(wm.get_active_window() == w1);
    
    // Verify creation order remains unchanged
    auto snaps2 = wm.get_window_snapshots();
    CHECK(snaps2.size() == 3);
    CHECK(snaps2[0].window_id == w1);
    CHECK(snaps2[1].window_id == w2);
    CHECK(snaps2[2].window_id == w3);
    
    // Destroy w2
    wm.destroy_window(w2);
    
    auto snaps3 = wm.get_window_snapshots();
    CHECK(snaps3.size() == 2);
    CHECK(snaps3[0].window_id == w1);
    CHECK(snaps3[1].window_id == w3);
    
    std::cout << "test_taskbar_enumeration passed\n";
}

void test_window_maximization() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    BasicWindowManager wm(event_bus, logger);
    
    wm.set_work_area({0, 0, 1920, 1040});
    
    uint32_t w1 = wm.create_window("Window 1", {10, 10, 200, 200});
    
    // Maximize
    wm.set_window_state(w1, WindowState::Maximized);
    CHECK(wm.get_window_state(w1) == WindowState::Maximized);
    
    auto geom = wm.get_window_geometry(w1);
    CHECK(geom->width == 1920);
    CHECK(geom->height == 1040);
    CHECK(geom->x == 0);
    CHECK(geom->y == 0);
    
    // Restore
    wm.set_window_state(w1, WindowState::Normal);
    CHECK(wm.get_window_state(w1) == WindowState::Normal);
    
    geom = wm.get_window_geometry(w1);
    CHECK(geom->width == 200);
    CHECK(geom->height == 200);
    CHECK(geom->x == 10);
    CHECK(geom->y == 10);
    
    // Maximize then Minimize then Restore
    wm.set_window_state(w1, WindowState::Maximized);
    wm.set_window_state(w1, WindowState::Minimized);
    CHECK(wm.get_window_state(w1) == WindowState::Minimized);
    
    wm.set_window_state(w1, WindowState::Normal);
    CHECK(wm.get_window_state(w1) == WindowState::Normal);
    geom = wm.get_window_geometry(w1);
    CHECK(geom->width == 200); // Should restore to original normal geometry!
    
    std::cout << "test_window_maximization passed\n";
}

int main() { std::cout << "Running Window Manager Orchestration Tests...\n";
    test_scene_compilation();
    test_stress_focus_transitions();
    test_resize_constraints();
    test_resize_hit_testing();
    test_taskbar_enumeration();
    test_window_maximization();
    std::cout << "All WindowManager tests passed successfully.\n";
    return 0;
}
