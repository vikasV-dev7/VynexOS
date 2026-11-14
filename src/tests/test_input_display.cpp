#include <atomic>
#include "test_check.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../hal/mock_input_driver.hpp"
#include "vynexos/hal/input_driver.hpp"
#include "vynexos/hal/viewport.hpp"

using namespace vynexos;

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

// Display whose logical size is configurable, to prove layout derives from the backend.
class SizedDisplay : public desktop::IDisplayAbstraction {
public:
    explicit SizedDisplay(hal::DisplaySize size) : m_size(size) {}
    std::expected<void, desktop::DisplayError> initialize() override { return {}; }
    std::expected<void, desktop::DisplayError> flush_buffer(const hal::FrameBuffer&) override { return {}; }
    void shutdown() override {}
    hal::DisplaySize logical_size() const override { return m_size; }
private:
    hal::DisplaySize m_size;
};

// ---------------------------------------------------------------- viewport math

void test_viewport_exact_aspect() {
    auto vp = hal::compute_viewport(1920, 1080, 1920, 1080);
    CHECK(vp.x == 0 && vp.y == 0 && vp.width == 1920 && vp.height == 1080);

    vp = hal::compute_viewport(960, 540, 1920, 1080);
    CHECK(vp.x == 0 && vp.y == 0 && vp.width == 960 && vp.height == 540);
    std::cout << "test_viewport_exact_aspect passed\n";
}

void test_viewport_letterbox_and_pillarbox() {
    // 4:3 window, 16:9 desktop -> bars above and below, aspect preserved (not stretched).
    auto vp = hal::compute_viewport(1024, 768, 1920, 1080);
    CHECK(vp.width == 1024 && vp.height == 576);
    CHECK(vp.x == 0 && vp.y == 96);

    // Wide window -> bars left and right.
    vp = hal::compute_viewport(2000, 500, 1920, 1080);
    CHECK(vp.height == 500 && vp.width == 889); // 500*1920/1080 = 888.9 -> 889
    CHECK(vp.x == 555 && vp.y == 0);

    // The viewport never exceeds the window.
    for (uint32_t w : {1u, 2u, 3u, 17u, 640u, 1280u, 3840u}) {
        for (uint32_t h : {1u, 2u, 5u, 99u, 720u, 2160u}) {
            auto v = hal::compute_viewport(w, h, 1920, 1080);
            CHECK(v.x >= 0 && v.y >= 0);
            CHECK(static_cast<uint32_t>(v.x) + v.width <= w);
            CHECK(static_cast<uint32_t>(v.y) + v.height <= h);
        }
    }
    std::cout << "test_viewport_letterbox_and_pillarbox passed\n";
}

void test_viewport_degenerate() {
    auto vp = hal::compute_viewport(0, 100, 1920, 1080);
    CHECK(vp.width == 0 && vp.height == 0);
    vp = hal::compute_viewport(100, 0, 1920, 1080);
    CHECK(vp.width == 0 && vp.height == 0);
    vp = hal::compute_viewport(100, 100, 0, 1080);
    CHECK(vp.width == 0 && vp.height == 0);

    // Mapping through an empty viewport is safe.
    auto pt = hal::window_to_logical(vp, 50, 50);
    CHECK(pt.x == 0 && pt.y == 0);

    // Very large values do not overflow.
    vp = hal::compute_viewport(0xFFFFFFFFu, 0xFFFFFFFFu, 1920, 1080);
    CHECK(vp.width > 0 && vp.height > 0);
    std::cout << "test_viewport_degenerate passed\n";
}

void test_pointer_mapping_boundaries() {
    const auto vp = hal::compute_viewport(1024, 768, 1920, 1080); // y offset 96, 1024x576

    // Top-left pixel of the viewport maps to the desktop origin.
    auto pt = hal::window_to_logical(vp, 0, 96);
    CHECK(pt.x == 0 && pt.y == 0);

    // Last pixel maps to the last desktop pixels (never one past the edge).
    pt = hal::window_to_logical(vp, 1023, 96 + 575);
    CHECK(pt.x == 1918 && pt.y == 1078);

    // Center maps to the center.
    pt = hal::window_to_logical(vp, 512, 384);
    CHECK(pt.x == 960 && pt.y == 540);

    // Letterbox bars and outside-window positions clamp to the nearest edge.
    pt = hal::window_to_logical(vp, 512, 10); // top bar
    CHECK(pt.y == 0 && pt.x == 960);
    pt = hal::window_to_logical(vp, 512, 760); // bottom bar
    CHECK(pt.y == 1078);
    pt = hal::window_to_logical(vp, -500, -500);
    CHECK(pt.x == 0 && pt.y == 0);
    pt = hal::window_to_logical(vp, 100000, 100000);
    CHECK(pt.x == 1918 && pt.y == 1078);

    // Mapping is monotonic and always inside the logical desktop.
    int32_t prev = -1;
    for (int32_t x = -5; x < 1030; ++x) {
        auto p = hal::window_to_logical(vp, x, 384);
        CHECK(p.x >= 0 && p.x < 1920);
        CHECK(p.x >= prev);
        prev = p.x;
    }
    std::cout << "test_pointer_mapping_boundaries passed\n";
}

void test_resized_window_mapping() {
    // The window center maps to the desktop center regardless of window size or aspect.
    const std::vector<std::pair<uint32_t, uint32_t>> sizes{{1280, 720}, {640, 360}, {1920, 1080}, {800, 800}, {3000, 600}};
    for (const auto& [w, h] : sizes) {
        const auto vp = hal::compute_viewport(w, h, 1920, 1080);
        const auto center = hal::window_to_logical(vp, static_cast<int32_t>(w / 2), static_cast<int32_t>(h / 2));
        CHECK(center.x >= 958 && center.x <= 962);
        CHECK(center.y >= 538 && center.y <= 542);
    }
    std::cout << "test_resized_window_mapping passed\n";
}

void test_high_dpi_mapping() {
    // Window reports 1024x768 units but renders 1536x1152 pixels (150% scale).
    CHECK(hal::window_units_to_pixels(0, 1024, 1536) == 0);
    CHECK(hal::window_units_to_pixels(512, 1024, 1536) == 768);
    CHECK(hal::window_units_to_pixels(1023, 1024, 1536) == 1534);
    CHECK(hal::window_units_to_pixels(100, 1024, 2048) == 200); // 200% scale
    CHECK(hal::window_units_to_pixels(42, 0, 100) == 42);       // degenerate: unchanged

    const auto vp = hal::compute_viewport(1536, 1152, 1920, 1080);
    // The window-unit center must still map to the desktop center.
    const int32_t px = hal::window_units_to_pixels(512, 1024, 1536);
    const int32_t py = hal::window_units_to_pixels(384, 768, 1152);
    const auto pt = hal::window_to_logical(vp, px, py);
    CHECK(pt.x == 960 && pt.y == 540);
    std::cout << "test_high_dpi_mapping passed\n";
}

// ---------------------------------------------------------------- event contract / ordering

void test_input_topic_contract() {
    CHECK(std::string(hal::kTopicInputKey) == "HAL_INPUT_KEY");
    CHECK(std::string(hal::kTopicInputMouse) == "HAL_INPUT_MOUSE");

    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto bus = std::make_shared<core::InMemoryEventBus>(scheduler);

    std::vector<std::string> topics;
    bus->subscribe(hal::kTopicInputKey, [&](std::shared_ptr<const core::Event> ev) { topics.push_back(ev->topic); });
    bus->subscribe(hal::kTopicInputMouse, [&](std::shared_ptr<const core::Event> ev) { topics.push_back(ev->topic); });

    // The mock driver is the reference publisher: what it emits is what consumers must subscribe to.
    hal::MockInputDriver driver(bus, logger);
    driver.inject_key(65, true);
    driver.inject_mouse(10, 20, 1);

    CHECK(topics.size() == 2); // synchronous: no waiting needed
    CHECK(topics[0] == "HAL_INPUT_KEY");
    CHECK(topics[1] == "HAL_INPUT_MOUSE");
    std::cout << "test_input_topic_contract passed\n";
}

void test_ordered_input_dispatch() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler(*logger, 8); // multiple workers: async publish gives no ordering
    auto bus = std::make_shared<core::InMemoryEventBus>(scheduler);

    const auto caller = std::this_thread::get_id();
    std::vector<int32_t> seen;
    bool wrong_thread = false;
    bus->subscribe(hal::kTopicInputMouse, [&](std::shared_ptr<const core::Event> ev) {
        if (std::this_thread::get_id() != caller) wrong_thread = true;
        seen.push_back(std::any_cast<hal::MouseEvent>(ev->payload).x);
    });

    hal::MockInputDriver driver(bus, logger);
    constexpr int kCount = 5000;
    for (int i = 0; i < kCount; ++i) {
        driver.inject_mouse(i, 0, static_cast<uint8_t>(i % 2));
    }

    // Everything was handled before inject_mouse returned, on this thread, strictly in order.
    CHECK(!wrong_thread);
    CHECK(seen.size() == static_cast<size_t>(kCount));
    for (int i = 0; i < kCount; ++i) {
        CHECK(seen[static_cast<size_t>(i)] == i);
    }

    // Other topics keep asynchronous publish() semantics.
    std::atomic<bool> got{false};
    std::atomic<bool> off_thread{false};
    bus->subscribe("unrelated_topic", [&](std::shared_ptr<const core::Event>) {
        off_thread = (std::this_thread::get_id() != caller);
        got = true;
    });
    auto ev = std::make_shared<core::Event>();
    ev->topic = "unrelated_topic";
    bus->publish(ev);
    for (int i = 0; i < 200 && !got; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(got);
    CHECK(off_thread);
    std::cout << "test_ordered_input_dispatch passed\n";
}

// ---------------------------------------------------------------- shell layout / input

struct ShellFixture {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    std::unique_ptr<core::BasicTaskScheduler> scheduler;
    std::shared_ptr<core::InMemoryEventBus> bus;
    std::shared_ptr<desktop::BasicWindowManager> wm;
    std::shared_ptr<desktop::BasicCompositor> compositor;
    std::shared_ptr<apps::DesktopShell> shell;

    explicit ShellFixture(hal::DisplaySize size) {
        scheduler = std::make_unique<core::BasicTaskScheduler>(*logger);
        bus = std::make_shared<core::InMemoryEventBus>(*scheduler);
        auto display = std::make_shared<SizedDisplay>(size);
        compositor = std::make_shared<desktop::BasicCompositor>(display, logger);
        wm = std::make_shared<desktop::BasicWindowManager>(bus, logger);
        auto toolkit = std::make_shared<desktop::BasicWidgetToolkit>();
        auto decorator = std::make_shared<desktop::BasicWindowDecorator>(toolkit);
        auto taskbar = std::make_shared<desktop::BasicTaskbar>(toolkit);
        shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        shell->launch();
    }

    std::optional<desktop::WindowSnapshot> find(const std::string& title) const {
        for (const auto& s : wm->get_window_snapshots()) {
            if (s.title == title) return s;
        }
        return std::nullopt;
    }
};

void test_shell_layout_follows_display_size() {
    ShellFixture f({1280, 720});
    CHECK(f.compositor->logical_size().width == 1280);
    CHECK(f.compositor->logical_size().height == 720);

    const auto work = f.wm->get_work_area();
    CHECK(work.x == 0 && work.y == 0 && work.width == 1280 && work.height == 672); // 720 less the 48 px taskbar

    auto taskbar = f.find("Taskbar");
    CHECK(taskbar.has_value());
    auto geom = f.wm->get_window_geometry(taskbar->window_id);
    CHECK(geom.has_value());
    CHECK(geom->x == 0 && geom->y == 672 && geom->width == 1280 && geom->height == 48);

    auto wallpaper = f.find("Wallpaper");
    CHECK(wallpaper.has_value());
    auto wgeom = f.wm->get_window_geometry(wallpaper->window_id);
    CHECK(wgeom.has_value());
    CHECK(wgeom->width == 1280 && wgeom->height == 720);
    std::cout << "test_shell_layout_follows_display_size passed\n";
}

void test_launcher_button_tracks_taskbar() {
    ShellFixture f({1280, 720});
    f.shell->update_frame();

    auto launcher_visible = [&] {
        auto l = f.find("Launcher");
        return l.has_value() && f.wm->is_window_visible(l->window_id);
    };
    CHECK(!launcher_visible());

    auto mouse = [&](int32_t x, int32_t y, uint8_t state) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputMouse;
        ev->payload = hal::MouseEvent(x, y, state);
        f.bus->dispatch_immediate(ev); // synchronous: deterministic, no sleeps
    };

    // The old hard-coded 1080p button location is not a button on a 720p desktop.
    mouse(20, 1060, 1);
    mouse(20, 1060, 0);
    CHECK(!launcher_visible());

    // The real button: taskbar top (672) + 6 .. +42, x 8..88.
    mouse(20, 695, 1);
    mouse(20, 695, 0);
    CHECK(launcher_visible());

    mouse(20, 695, 1);
    mouse(20, 695, 0);
    CHECK(!launcher_visible());
    std::cout << "test_launcher_button_tracks_taskbar passed\n";
}

void test_launcher_geometry_follows_the_tokens() {
    // The launcher is the token size (420 x 520), one 12 px margin from the left edge and from the taskbar (48 px).
    struct Case {
        hal::DisplaySize display;
        int32_t x, y;
        uint32_t width, height;
    };
    const Case cases[] = {
        {{1920, 1080}, 12, 500, 420, 520}, // 1080 - 48 - 12 - 520
        {{1280, 720}, 12, 140, 420, 520},  // 720 - 48 - 12 - 520
        // Too small for the token size: it shrinks to the space above the taskbar, a margin from every edge, and
        // never starts above the top of the screen.
        {{640, 480}, 12, 12, 420, 408},    // height 480 - 48 - 2 * 12
        {{320, 180}, 12, 12, 296, 108},    // width 320 - 2 * 12, height 180 - 48 - 2 * 12
    };
    for (const Case& c : cases) {
        ShellFixture f(c.display);
        const auto launcher = f.find("Launcher");
        CHECK(launcher.has_value());
        const auto g = f.wm->get_window_geometry(launcher->window_id);
        CHECK(g.has_value());
        CHECK(g->x == c.x && g->y == c.y && g->width == c.width && g->height == c.height);
        const auto work = f.wm->get_work_area();
        CHECK(work.height == c.display.height - 48); // the work area ends where the 48 px taskbar begins
        CHECK(g->y >= 0 && g->y + static_cast<int32_t>(g->height) <= static_cast<int32_t>(work.height)); // on screen, above the taskbar
        CHECK(g->x >= 0 && g->x + static_cast<int32_t>(g->width) <= static_cast<int32_t>(c.display.width));
        const auto taskbar = f.find("Taskbar");
        CHECK(taskbar.has_value());
        const auto t = f.wm->get_window_geometry(taskbar->window_id);
        CHECK(t.has_value() && t->height == 48 && t->y == static_cast<int32_t>(c.display.height) - 48 && t->width == c.display.width);
    }
    std::cout << "test_launcher_geometry_follows_the_tokens passed\n";
}

int main() {
    std::cout << "Running Input & Display Tests...\n";
    test_viewport_exact_aspect();
    test_viewport_letterbox_and_pillarbox();
    test_viewport_degenerate();
    test_pointer_mapping_boundaries();
    test_resized_window_mapping();
    test_high_dpi_mapping();
    test_input_topic_contract();
    test_ordered_input_dispatch();
    test_shell_layout_follows_display_size();
    test_launcher_button_tracks_taskbar();
    test_launcher_geometry_follows_the_tokens();
    std::cout << "All input/display tests passed successfully.\n";
    return 0;
}
