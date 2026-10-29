#include <iostream>
#include <cassert>
#include <memory>
#include <string>
#include "../apps/desktop_shell.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_taskbar.hpp"
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
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    
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
    // Verify panel background color {45, 45, 45, 255} over {0, 0, 0, 0}
    // Result = 45
    assert(task_map[0] == 45); // R
    assert(task_map[1] == 45); // G
    assert(task_map[2] == 45); // B
    assert(task_map[3] == 255); // A
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
    
    // Simulate release
    auto release1 = std::make_shared<core::Event>();
    release1->topic = "HAL_INPUT_MOUSE";
    release1->payload = hal::MouseEvent(50, 1060, 0);
    event_bus->publish(release1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

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
    std::string last_topic;
    
    bool initialize() override { initialized = true; return true; }
    void start() override {}
    void update(double /*delta_time*/) override { updated = true; }
    void render(vynexos::desktop::ISurface& /*surface*/) override { rendered = true; }
    void shutdown() override { shutdown_called = true; }
    bool handle_event(const vynexos::core::Event& event) override { event_handled = true; last_topic = event.topic; return true; }
    [[nodiscard]] std::string get_name() const noexcept override { return "MockApp"; }
};

void test_runtime_ownership() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    shell.launch();
    
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
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    shell.launch();
    
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
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
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
    
    // Release
    auto release_empty = std::make_shared<core::Event>();
    release_empty->topic = "HAL_INPUT_MOUSE";
    release_empty->payload = hal::MouseEvent(800, 800, 0);
    event_bus->publish(release_empty);
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
    
    // Release before clicking again
    auto release_app = std::make_shared<core::Event>();
    release_app->topic = "HAL_INPUT_MOUSE";
    release_app->payload = hal::MouseEvent(150, 150, 0);
    event_bus->publish(release_app);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Click on already focused window should not change runtime state negatively
    event_bus->publish(click_app);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(shell.has_focus());
    assert(shell.focused_window().value() == win_id);
    
    // Keyboard event should be routed to focused app
    // The topic must be the exact one published by the real input drivers.
    assert(std::string(hal::kTopicInputKey) == "HAL_INPUT_KEY");

    // Earlier mouse clicks were also routed to the app; clear that state so the keyboard
    // assertions below can only be satisfied by keyboard events.
    app_ptr->event_handled = false;
    app_ptr->last_topic.clear();

    // A legacy/incorrect topic must not reach the application.
    auto wrong_topic_ev = std::make_shared<core::Event>();
    wrong_topic_ev->topic = "HAL_INPUT_KEYBOARD";
    wrong_topic_ev->payload = hal::KeyEvent(65, true);
    event_bus->publish(wrong_topic_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!app_ptr->event_handled);

    auto key_ev = std::make_shared<core::Event>();
    key_ev->topic = hal::kTopicInputKey;
    key_ev->payload = hal::KeyEvent(65, true);
    event_bus->publish(key_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(app_ptr->event_handled);
    assert(app_ptr->last_topic == hal::kTopicInputKey);
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

void test_drag_routing() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    shell.launch();
    
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
    
    // 1. Mouse Button Press (Drag Start)
    auto click_ev = std::make_shared<core::Event>();
    click_ev->topic = "HAL_INPUT_MOUSE";
    click_ev->payload = hal::MouseEvent(150, 80, 1); // 80 is in TitleBar (60 to 100)
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    assert(shell.focused_window().has_value() && shell.focused_window().value() == win_id);
    
    // 2. Mouse Move (Below threshold, should not trigger move)
    auto move_small = std::make_shared<core::Event>();
    move_small->topic = "HAL_INPUT_MOUSE";
    move_small->payload = hal::MouseEvent(151, 81, 1);
    event_bus->publish(move_small);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_before = wm->get_window_geometry(win_id).value();
    assert(geom_before.x == 100 && geom_before.y == 100);
    
    // 3. Mouse Move (Above threshold, should trigger capture and move)
    auto move_large = std::make_shared<core::Event>();
    move_large->topic = "HAL_INPUT_MOUSE";
    move_large->payload = hal::MouseEvent(200, 180, 1); // delta: 50, 100
    event_bus->publish(move_large);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_after = wm->get_window_geometry(win_id).value();
    if (!(geom_after.x == 150 && geom_after.y == 200)) {
        printf("FAILED! geom_after.x=%d, geom_after.y=%d\n", geom_after.x, geom_after.y);
    }
    assert(geom_after.x == 150 && geom_after.y == 200); // 100+50, 100+100
    
    // 4. Mouse Move Outside Window (Pointer Capture test)
    auto move_out = std::make_shared<core::Event>();
    move_out->topic = "HAL_INPUT_MOUSE";
    move_out->payload = hal::MouseEvent(800, 800, 1); // delta: 650, 720 from 150, 80
    app_ptr->event_handled = false;
    event_bus->publish(move_out);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_out = wm->get_window_geometry(win_id).value();
    assert(geom_out.x == 750 && geom_out.y == 820); // 100+650, 100+720
    assert(app_ptr->event_handled == true); // Pointer captured, event still routed!
    
    // 5. Mouse Release
    auto release_ev = std::make_shared<core::Event>();
    release_ev->topic = "HAL_INPUT_MOUSE";
    release_ev->payload = hal::MouseEvent(800, 800, 0);
    event_bus->publish(release_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // 6. Mouse Move after release (no drag)
    auto move_after = std::make_shared<core::Event>();
    move_after->topic = "HAL_INPUT_MOUSE";
    move_after->payload = hal::MouseEvent(850, 850, 0);
    event_bus->publish(move_after);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_final = wm->get_window_geometry(win_id).value();
    assert(geom_final.x == 750 && geom_final.y == 820); // Should not change
    
    std::cout << "test_drag_routing passed\n";
}

void test_resize_routing() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    shell.launch();
    
    auto app = std::make_unique<MockRuntimeApp>();
    auto* app_ptr = app.get();
    
    // Window starts at x:100, y:100, width:400, height:300
    uint32_t win_id = wm->create_window("Mock App", desktop::WindowGeometry{100, 100, 400, 300});
    
    vynexos::desktop::RunningApplication running_app;
    running_app.app = app_ptr;
    running_app.window_id = win_id;
    running_app.surface = compositor->create_surface(400, 300);
    running_app.visible = true;
    
    wm->set_window_surface(win_id, running_app.surface);
    shell.add_running_application(std::move(running_app));
    
    // 1. Mouse Button Press on Right Edge (center of 5px border inside the 505 bounds)
    auto click_ev = std::make_shared<core::Event>();
    click_ev->topic = "HAL_INPUT_MOUSE";
    click_ev->payload = hal::MouseEvent(502, 250, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    assert(shell.focused_window().has_value() && shell.focused_window().value() == win_id);
    
    // 2. Mouse Move (Below threshold, should not trigger resize)
    auto move_small = std::make_shared<core::Event>();
    move_small->topic = "HAL_INPUT_MOUSE";
    move_small->payload = hal::MouseEvent(503, 250, 1); // delta: 1
    event_bus->publish(move_small);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_before = wm->get_window_geometry(win_id).value();
    assert(geom_before.width == 400);
    
    // 3. Mouse Move (Above threshold, should trigger capture and resize)
    auto move_large = std::make_shared<core::Event>();
    move_large->topic = "HAL_INPUT_MOUSE";
    move_large->payload = hal::MouseEvent(552, 250, 1); // delta: 50, 0
    event_bus->publish(move_large);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_after = wm->get_window_geometry(win_id).value();
    assert(geom_after.width == 450); // 400+50
    assert(geom_after.height == 300); // Unchanged
    assert(geom_after.x == 100);
    
    // 4. Test TopLeft corner resize
    // Release
    auto release_ev = std::make_shared<core::Event>();
    release_ev->topic = "HAL_INPUT_MOUSE";
    release_ev->payload = hal::MouseEvent(552, 250, 0);
    event_bus->publish(release_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Click on TopLeft edge (x=100, y=100)
    auto click2_ev = std::make_shared<core::Event>();
    click2_ev->topic = "HAL_INPUT_MOUSE";
    click2_ev->payload = hal::MouseEvent(95, 60, 1);
    event_bus->publish(click2_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Move TopLeft inside by 20,20
    auto move2 = std::make_shared<core::Event>();
    move2->topic = "HAL_INPUT_MOUSE";
    move2->payload = hal::MouseEvent(115, 80, 1); // delta: +20, +20
    event_bus->publish(move2);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto geom_final = wm->get_window_geometry(win_id).value();
    assert(geom_final.x == 120); // 100+20
    assert(geom_final.y == 120); // 100+20
    assert(geom_final.width == 430); // 450 - 20
    assert(geom_final.height == 280); // 300 - 20
    
    std::cout << "test_resize_routing passed\n";
}

void test_window_state_management() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit); DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    shell.launch();
    
    auto app = std::make_unique<MockRuntimeApp>();
    auto* app_ptr = app.get();
    
    // Create window
    uint32_t win_id = wm->create_window("Mock App", desktop::WindowGeometry{100, 100, 400, 300});
    
    vynexos::desktop::RunningApplication running_app;
    running_app.app = app_ptr;
    running_app.window_id = win_id;
    running_app.surface = compositor->create_surface(400, 300);
    running_app.visible = true;
    
    wm->set_window_surface(win_id, running_app.surface);
    wm->set_window_decoration(win_id, compositor->create_surface(400, 300), WindowMargins{40, 5, 5, 5});
    shell.add_running_application(std::move(running_app));
    
    // Assert Initial State
    assert(wm->get_window_state(win_id) == WindowState::Normal);
    
    // Give focus
    shell.focus_window(win_id);
    assert(shell.focused_window().has_value() && shell.focused_window().value() == win_id);
    
    // Click Minimize Button (Width - 80 to Width - 50, Y: 5 to 35)
    // Absolute position: x = 95 + 410 - 65 = 440, y = 60 + 20 = 80
    auto click_ev = std::make_shared<core::Event>();
    click_ev->topic = "HAL_INPUT_MOUSE";
    click_ev->payload = hal::MouseEvent(440, 80, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto release_ev = std::make_shared<core::Event>();
    release_ev->topic = "HAL_INPUT_MOUSE";
    release_ev->payload = hal::MouseEvent(440, 80, 0); // Release should trigger
    event_bus->publish(release_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Verify window is Minimized
    assert(wm->get_window_state(win_id) == WindowState::Minimized);
    
    // Verify scene graph does NOT contain the minimized window content/decorations
    auto scene = wm->build_scene();
    for (const auto& layer : scene) {
        assert(layer.window_id != win_id); // The window should be omitted completely
    }
    
    // Verify focus was removed or transferred (in this case, only Taskbar/Wallpaper exist)
    // Actually our test adds wallpaper and taskbar by default. Focus will be transferred to one of them if eligible, or cleared.
    // Wallpaper and Taskbar have window IDs. Taskbar is highest Z order of remaining.
    // So focused window should not be win_id anymore.
    if (shell.focused_window().has_value()) {
        assert(shell.focused_window().value() != win_id);
    }
    
    // Restore window via API
    wm->set_window_state(win_id, WindowState::Normal);
    assert(wm->get_window_state(win_id) == WindowState::Normal);
    
    // Verify scene graph HAS the window again
    auto scene_after = wm->build_scene();
    bool found = false;
    for (const auto& layer : scene_after) {
        if (layer.window_id == win_id) {
            found = true;
            break;
        }
    }
    assert(found);
    
    std::cout << "test_window_state_management passed\n";
}

void test_taskbar_routing() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto compositor = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit);
    
    DesktopShell shell(wm, compositor, toolkit, decorator, taskbar, event_bus);
    shell.launch();
    
    // Create an app
    uint32_t win_id = wm->create_window("App 1", {100, 100, 400, 300});
    desktop::RunningApplication running_app;
    running_app.window_id = win_id;
    running_app.surface = compositor->create_surface(400, 300);
    wm->set_window_surface(win_id, running_app.surface);
    shell.add_running_application(std::move(running_app));
    
    shell.update_frame();
    
    // Simulate clicking Taskbar Button 1 (X: 100 to 250, Y: 5 to 35)
    auto click_ev = std::make_shared<core::Event>();
    click_ev->topic = "HAL_INPUT_MOUSE";
    // Taskbar geometry is usually (0, 1040, 1920, 40)
    // Click at (150, 1060) which maps to rel_x=150, rel_y=20
    click_ev->payload = hal::MouseEvent(150, 1060, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Check if window is focused
    assert(shell.focused_window().has_value() && shell.focused_window().value() == win_id);
    
    // Minimize the window
    wm->set_window_state(win_id, WindowState::Minimized);
    assert(wm->get_window_state(win_id) == WindowState::Minimized);
    
    // Release first click
    click_ev->payload = hal::MouseEvent(150, 1060, 0);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Click taskbar button again
    click_ev->payload = hal::MouseEvent(150, 1060, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Should be restored
    assert(wm->get_window_state(win_id) == WindowState::Normal);
    
    // Release second click
    click_ev->payload = hal::MouseEvent(150, 1060, 0);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Click taskbar button again (while focused)
    click_ev->payload = hal::MouseEvent(150, 1060, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Should be minimized again
    assert(wm->get_window_state(win_id) == WindowState::Minimized);
    
    std::cout << "test_taskbar_routing passed\n";
}

void test_maximize_routing() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto event_bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<MockDisplayBackend>(logger);
    auto comp = std::make_shared<BasicCompositor>(display, logger);
    auto wm = std::make_shared<BasicWindowManager>(event_bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto dec = std::make_shared<BasicWindowDecorator>(toolkit);
    auto taskbar = std::make_shared<BasicTaskbar>(toolkit);
    
    DesktopShell shell(wm, comp, toolkit, dec, taskbar, event_bus);
    shell.launch();
    
    desktop::RunningApplication running_app;
    running_app.app = nullptr;
    running_app.visible = true;
    
    uint32_t win_id = wm->create_window("App", {100, 100, 400, 300});
    running_app.window_id = win_id;
    running_app.surface = comp->create_surface(400, 300);
    wm->set_window_surface(win_id, running_app.surface);
    shell.add_running_application(std::move(running_app));
    
    shell.update_frame();
    
    // Simulate Maximize Button click: Width - 120 to Width - 90, Y: 5 to 35
    // Window is at 100, 100 with width 400.
    // Button is at rel_x = 305, rel_y = 20. Abs = 400, 80
    
    auto click_ev = std::make_shared<core::Event>();
    click_ev->topic = "HAL_INPUT_MOUSE";
    click_ev->payload = hal::MouseEvent(400, 80, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    assert(wm->get_window_state(win_id) == WindowState::Maximized);
    
    // Try to drag (click TitleBar)
    click_ev->payload = hal::MouseEvent(400, 80, 0); // Release Maximize button
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Click title bar at abs 150, 110 (which is now in the maximized window's title bar, 150, 10 relative)
    click_ev->payload = hal::MouseEvent(150, 10, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Drag
    click_ev->payload = hal::MouseEvent(250, 10, 1);
    event_bus->publish(click_ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Window should NOT have moved because drag is disabled when maximized
    auto geom = wm->get_window_geometry(win_id);
    assert(geom->x == 5); // Still at 5 (work_area.x + margin.left) because it's maximized
    
    std::cout << "test_maximize_routing passed\n";
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    std::cout << "Running Desktop Shell Tests...\n";
    test_desktop_shell_rendering();
    test_runtime_ownership();
    test_hidden_applications_are_not_rendered();
    test_focus_routing();
    test_drag_routing();
    test_resize_routing();
    test_window_state_management();
    test_taskbar_routing();
    test_maximize_routing();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
