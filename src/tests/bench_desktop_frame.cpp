// Desktop frame-cost benchmark (not a pass/fail test; intentionally not registered with CTest).
//
// Mirrors the CompositionRoot frame loop - shell/app update, scene build, compositor render - and
// reports per-phase timings for three workloads:
//   idle        nothing changes (steady state of the real loop)
//   full_dirty  the whole screen is invalidated every frame (worst case)
//   drag        a window is dragged through the real shell input path
//
// Usage: bench_desktop_frame [--frames N] [--sdl]
//   --sdl  presents through the real SDL2 backend (opens a window; presentation is vsync-paced,
//          so the "render" phase then includes waiting for vertical sync).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "../apps/basic_file_explorer.hpp"
#include "../apps/basic_terminal.hpp"
#include "../apps/demo_app.hpp"
#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/mock_display_backend.hpp"
#ifdef VYNEX_ENABLE_SDL2
#include "../desktop/sdl2_display_backend.hpp"
#endif
#include "vynexos/core/memory_file_system.hpp"
#include "vynexos/hal/input_driver.hpp"

using namespace vynexos;
using Clock = std::chrono::steady_clock;

namespace {

class NullLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

struct Stats {
    double mean{}, p50{}, p95{}, p99{}, max{};
};

Stats summarize(std::vector<double> v) {
    Stats s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (double x : v) sum += x;
    auto pct = [&](double p) { return v[std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size())))]; };
    s.mean = sum / static_cast<double>(v.size());
    s.p50 = pct(0.50);
    s.p95 = pct(0.95);
    s.p99 = pct(0.99);
    s.max = v.back();
    return s;
}

void print_row(const char* label, const Stats& s) {
    std::printf("    %-8s mean %8.3f  p50 %8.3f  p95 %8.3f  p99 %8.3f  max %8.3f  (ms)\n",
                label, s.mean, s.p50, s.p95, s.p99, s.max);
}

} // namespace

int main(int argc, char** argv) {
    int frames = 600;
    bool use_sdl = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = std::atoi(argv[++i]);
        if (std::strcmp(argv[i], "--sdl") == 0) use_sdl = true;
    }

    auto logger = std::make_shared<NullLogger>();
    core::BasicTaskScheduler scheduler(*logger);
    auto bus = std::make_shared<core::InMemoryEventBus>(scheduler);

    std::shared_ptr<desktop::IDisplayAbstraction> display;
    if (use_sdl) {
#ifdef VYNEX_ENABLE_SDL2
        auto sdl = std::make_shared<desktop::SDL2DisplayBackend>(logger);
        if (!sdl->initialize()) {
            std::cerr << "SDL display init failed\n";
            return 1;
        }
        display = sdl;
#else
        std::cerr << "built without SDL2\n";
        return 1;
#endif
    } else {
        auto mock = std::make_shared<desktop::MockDisplayBackend>(logger);
        (void)mock->initialize();
        display = mock;
    }

    auto compositor = std::make_shared<desktop::BasicCompositor>(display, logger);
    auto wm = std::make_shared<desktop::BasicWindowManager>(bus, logger);
    auto toolkit = std::make_shared<desktop::BasicWidgetToolkit>();
    auto decorator = std::make_shared<desktop::BasicWindowDecorator>(toolkit);
    auto taskbar = std::make_shared<desktop::BasicTaskbar>(toolkit);
    auto shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
    auto vfs = std::make_shared<core::MemoryFileSystem>();
    auto terminal = std::make_shared<apps::BasicTerminal>(wm, compositor, toolkit);
    auto demo = std::make_shared<apps::DemoApp>(wm, compositor, toolkit);

    // Same population as CompositionRoot::initialize().
    shell->launch();
    terminal->launch();
    apps::BasicFileExplorer explorer(vfs, toolkit);
    {
        desktop::WindowGeometry geom{200, 200, 700, 500};
        const uint32_t win_id = wm->create_window("File Explorer", geom);
        auto surface = compositor->create_surface(geom.width, geom.height);
        wm->set_window_surface(win_id, surface);
        desktop::RunningApplication app;
        app.app = &explorer;
        app.window_id = win_id;
        app.surface = surface;
        app.visible = true;
        shell->add_running_application(std::move(app));
    }
    demo->launch();

    const auto size = compositor->logical_size();
    std::printf("Desktop frame benchmark: %ux%u logical, %d frames/scenario, display=%s\n",
#ifdef NDEBUG
                size.width, size.height, frames, use_sdl ? "SDL2 (vsync)" : "mock");
    std::printf("Build: NDEBUG (optimized)\n");
#else
                size.width, size.height, frames, use_sdl ? "SDL2 (vsync)" : "mock");
    std::printf("Build: Debug\n");
#endif

    struct Phases {
        std::vector<double> update, scene, render, total;
    };

    auto run_frame = [&](Phases& p) {
        const auto t_frame = Clock::now();
        auto t = Clock::now();
        shell->update_frame();
        terminal->update_frame();
        demo->update_frame();
        p.update.push_back(ms_since(t));

        t = Clock::now();
        auto scene = wm->build_scene();
        p.scene.push_back(ms_since(t));

        t = Clock::now();
        (void)compositor->render_frame(scene);
        p.render.push_back(ms_since(t));
        p.total.push_back(ms_since(t_frame));
    };

    auto report = [&](const char* name, Phases& p, const char* note) {
        std::printf("\n  [%s] %s\n", name, note);
        print_row("update", summarize(p.update));
        print_row("scene", summarize(p.scene));
        print_row("render", summarize(p.render));
        print_row("TOTAL", summarize(p.total));
    };

    auto mouse = [&](int32_t x, int32_t y, uint8_t state) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputMouse;
        ev->payload = hal::MouseEvent(x, y, state);
        bus->dispatch_immediate(ev);
    };

    Phases warm;
    for (int i = 0; i < 30; ++i) run_frame(warm); // warm-up; first frame is a full redraw

    // 1. idle
    {
        Phases p;
        for (int i = 0; i < frames; ++i) run_frame(p);
        report("idle", p, "steady state, nothing changes");
    }

    // 2. full-screen dirty every frame
    {
        Phases p;
        for (int i = 0; i < frames; ++i) {
            compositor->invalidate_region({0, 0, size.width, size.height});
            run_frame(p);
        }
        report("full_dirty", p, "entire desktop invalidated every frame (worst case)");
    }

    // 3. drag the File Explorer through the real shell input path
    {
        Phases p;
        mouse(500, 180, 1); // press on the Explorer title bar
        for (int i = 0; i < frames; ++i) {
            const double a = static_cast<double>(i) / 20.0;
            mouse(500 + static_cast<int32_t>(150.0 * std::sin(a)), 180 + static_cast<int32_t>(80.0 * std::cos(a)), 1);
            run_frame(p);
        }
        mouse(500, 180, 0);
        report("drag", p, "File Explorer dragged in a circle (shell input + dirty regions)");
    }

    shell.reset();
    std::printf("\nDone.\n");
    return 0;
}
