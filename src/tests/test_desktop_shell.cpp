#include <iostream>
#include <cassert>
#include <memory>
#include "../apps/desktop_shell.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../desktop/mock_display_backend.hpp"
#include "vynexos/hal/input_driver.hpp"

using namespace vynexos;
using namespace vynexos::desktop;
using namespace vynexos::apps;

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel level) override { (void)level; }
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel level, std::string_view message) override { (void)level; (void)message; }
};

void test_desktop_shell_rendering() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    
    DesktopShell shell(wm, compositor, toolkit, event_bus);
    
    // Test surface allocation and window registration
    shell.launch();
    
    auto z_order = wm->get_windows_z_ordered();
    assert(z_order.size() == 3); // Wallpaper, Taskbar, Launcher
    
    auto scene = wm->build_scene();
    assert(scene.size() == 2);
    assert(scene[0].surface != nullptr);
    assert(scene[1].surface != nullptr);
    
    // Draw the shell
    shell.update_frame();
    
    // Re-verify that drawing happened correctly natively
    auto wall_map = scene[0].surface->map_pixels();
    assert(wall_map[0] == 30); // Wallpaper dark grey R
    assert(wall_map[1] == 30); // Wallpaper dark grey G
    assert(wall_map[2] == 30); // Wallpaper dark grey B
    scene[0].surface->unmap_pixels();
    
    auto task_map = scene[1].surface->map_pixels();
    // Verify panel background color {45, 45, 45, 240} over {0, 0, 0, 0}
    // Result = 45 * 240 / 255 = 42
    assert(task_map[0] == 42); // R
    assert(task_map[1] == 42); // G
    assert(task_map[2] == 42); // B
    assert(task_map[3] == 240); // A
    scene[1].surface->unmap_pixels();
    
    // Launcher is initially invisible
    auto verify_launcher = [&]() -> bool {
        auto current_scene = wm->build_scene();
        return current_scene.size() == 3; // Wallpaper, Taskbar, Launcher (if visible)
    };
    
    assert(!verify_launcher());
    
    // Simulate valid click on Launcher button bounding box
    auto click_event = std::make_shared<core::Event>();
    click_event->topic = "HAL_INPUT_MOUSE";
    click_event->payload = hal::MouseEvent(50, 1060, 1);
    event_bus->publish(click_event);
    
    // Yield to let TaskScheduler process it
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(verify_launcher()); // Should now be visible
    
    // Simulate another click
    auto click2 = std::make_shared<core::Event>();
    click2->topic = "HAL_INPUT_MOUSE";
    click2->payload = hal::MouseEvent(50, 1060, 1);
    event_bus->publish(click2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(!verify_launcher()); // Should be hidden again
    
    std::cout << "test_desktop_shell_rendering passed\n";
}

// Mock application for runtime tests
class MockRuntimeApp : public vynexos::apps::IApplication {
public:
    bool initialized = false;
    bool updated = false;
    bool rendered = false;
    bool shutdown_called = false;
    bool event_handled = false;
    
    void initialize() override { initialized = true; }
    void start() override {}
#include <iostream>
#include <cassert>
#include <memory>
#include "../apps/desktop_shell.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../desktop/mock_display_backend.hpp"
#include "vynexos/hal/input_driver.hpp"

using namespace vynexos;
using namespace vynexos::desktop;
using namespace vynexos::apps;

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel level) override { (void)level; }
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel level, std::string_view message) override { (void)level; (void)message; }
};

void test_desktop_shell_rendering() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    
    DesktopShell shell(wm, compositor, toolkit, event_bus);
    
    // Test surface allocation and window registration
    shell.launch();
    
    auto z_order = wm->get_windows_z_ordered();
    assert(z_order.size() == 3); // Wallpaper, Taskbar, Launcher
    
    auto scene = wm->build_scene();
    assert(scene.size() == 2);
    assert(scene[0].surface != nullptr);
    assert(scene[1].surface != nullptr);
    
    // Draw the shell
    shell.update_frame();
    
    // Re-verify that drawing happened correctly natively
    auto wall_map = scene[0].surface->map_pixels();
    assert(wall_map[0] == 30); // Wallpaper dark grey R
    assert(wall_map[1] == 30); // Wallpaper dark grey G
    assert(wall_map[2] == 30); // Wallpaper dark grey B
    scene[0].surface->unmap_pixels();
    
    auto task_map = scene[1].surface->map_pixels();
    // Verify panel background color {45, 45, 45, 240} over {0, 0, 0, 0}
    // Result = 45 * 240 / 255 = 42
    assert(task_map[0] == 42); // R
    assert(task_map[1] == 42); // G
    assert(task_map[2] == 42); // B
    assert(task_map[3] == 240); // A
    scene[1].surface->unmap_pixels();
    
    // Launcher is initially invisible
    auto verify_launcher = [&]() -> bool {
        auto current_scene = wm->build_scene();
        return current_scene.size() == 3; // Wallpaper, Taskbar, Launcher (if visible)
    };
    
    assert(!verify_launcher());
    
    // Simulate valid click on Launcher button bounding box
    auto click_event = std::make_shared<core::Event>();
    click_event->topic = "HAL_INPUT_MOUSE";
    click_event->payload = hal::MouseEvent(50, 1060, 1);
    event_bus->publish(click_event);
    
    // Yield to let TaskScheduler process it
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(verify_launcher()); // Should now be visible
    
    // Simulate another click
    auto click2 = std::make_shared<core::Event>();
    click2->topic = "HAL_INPUT_MOUSE";
    click2->payload = hal::MouseEvent(50, 1060, 1);
    event_bus->publish(click2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(!verify_launcher()); // Should be hidden again
    
    std::cout << "test_desktop_shell_rendering passed\n";
}

// Mock application for runtime tests
class MockRuntimeApp : public vynexos::apps::IApplication {
public:
    bool initialized = false;
    bool updated = false;
    bool rendered = false;
    bool shutdown_called = false;
    bool event_handled = false;
    
    void initialize() override { initialized = true; }
    void start() override {}
    void update(double delta_time) override { updated = true; }
    void render(vynexos::desktop::ISurface& surface) override { rendered = true; }
    void shutdown() override { shutdown_called = true; }
    void handle_event(const vynexos::core::Event& event) override { event_handled = true; }
};

void test_runtime_ownership() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    
    DesktopShell shell(wm, compositor, toolkit, event_bus);
    
    auto app = std::make_unique<MockRuntimeApp>();
    auto* app_ptr = app.get();
    
    vynexos::desktop::RunningApplication running_app;
    running_app.app = app_ptr;
    running_app.window_id = 1;
    running_app.surface = compositor->create_surface(800, 600);
    running_app.visible = true;
    
    shell.add_running_application(std::move(running_app));
    assert(shell.running_application_count() == 1);
    
    // Update frame should dispatch update and render
    shell.update_frame();
    
    assert(app_ptr->updated);
    assert(app_ptr->rendered);
    
    // Test event routing
    auto test_event = std::make_shared<core::Event>();
    test_event->topic = "MOCK_EVENT";
    shell.handle_event(test_event.get());
    assert(app_ptr->event_handled);
    
    // Test cleanup
    shell.clear_running_applications();
    assert(shell.running_application_count() == 0);
    
    std::cout << "test_runtime_ownership passed\n";
}

void test_hidden_applications_are_not_rendered() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    
    DesktopShell shell(wm, compositor, toolkit, event_bus);
    
    auto app = std::make_unique<MockRuntimeApp>();
    auto* app_ptr = app.get();
    
    vynexos::desktop::RunningApplication running_app;
    running_app.app = app_ptr;
    running_app.window_id = 1;
    running_app.surface = compositor->create_surface(800, 600);
    running_app.visible = false; // Hidden!
    
    shell.add_running_application(std::move(running_app));
    
    shell.update_frame();
    
    assert(app_ptr->updated);   // Should still update
    assert(!app_ptr->rendered); // Should NOT render
    
    std::cout << "test_hidden_applications_are_not_rendered passed\n";
}

void test_focus_routing() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    
    DesktopShell shell(wm, compositor, toolkit, event_bus);
    shell.launch();
    
    // Create an app
    auto app = std::make_unique<MockRuntimeApp>();
    auto* app_ptr = app.get();
    
    uint32_t win_id = wm->create_window("Mock App", desktop::WindowGeometry{100, 100, 400, 300});
    
    vynexos::desktop::RunningApplication running_app;
    running_app.app = app_ptr;
    running_app.window_id = win_id;
    running_app.surface = compositor->create_surface(400, 300);
    running_app.visible = true;
    
    wm->set_window_surface(win_id, running_app.surface);
    shell.add_running_application(std::move(running_app));
    
    assert(!shell.focused_window().has_value());
    assert(shell.focused_application() == nullptr);
    
    // Click on empty space
    auto click_empty = std::make_shared<core::Event>();
    click_empty->topic = "HAL_INPUT_MOUSE";
    click_empty->payload = hal::MouseEvent(800, 800, 1);
    event_bus->publish(click_empty);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!shell.focused_window().has_value()); // Should remain unfocused or clear focus
    assert(!shell.has_focus());
    
    // Click on the app window (150, 150 is within 100,100 -> 500,400)
    auto click_app = std::make_shared<core::Event>();
    click_app->topic = "HAL_INPUT_MOUSE";
    click_app->payload = hal::MouseEvent(150, 150, 1);
    event_bus->publish(click_app);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    assert(shell.focused_window().has_value());
    assert(shell.has_focus());
    assert(shell.focused_window().value() == win_id);
    assert(shell.focused_application() != nullptr);
    assert(shell.focused_application()->focused == true);
    assert(app_ptr->event_handled);
    app_ptr->event_handled = false; // Reset
    
    // Click on already focused window should not change runtime state negatively
    event_bus->publish(click_app);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(shell.has_focus());
    assert(shell.focused_window().value() == win_id);
    
    // Keyboard event should be routed to focused app
    auto key_ev = std::make_shared<core::Event>();
    key_ev->topic = "HAL_INPUT_KEYBOARD";
    event_bus->publish(key_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(app_ptr->event_handled);
    app_ptr->event_handled = false; // Reset
    
    // Test that destroying focused application clears focus safely
    shell.remove_running_application(win_id);
    assert(!shell.has_focus());
    assert(!shell.focused_window().has_value());
    assert(shell.running_application_count() == 0);
    
    // Keyboard event should NOT be routed if no focus
    event_bus->publish(key_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!app_ptr->event_handled);
    
    std::cout << "test_focus_routing passed\n";
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    std::cout << "Running Desktop Shell Tests...\n";
    test_desktop_shell_rendering();
    test_runtime_ownership();
    test_hidden_applications_are_not_rendered();
    test_focus_routing();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
