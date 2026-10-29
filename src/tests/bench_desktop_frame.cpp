// Desktop frame-cost benchmark (not a pass/fail test; intentionally not registered with CTest).
//
// Mirrors the CompositionRoot frame loop - shell/app update, scene build, compositor render - and
// reports per-phase timings for three workloads:
//   idle        nothing changes (steady state of the real loop)
//   full_dirty  the whole screen is invalidated every frame (worst case)
//   drag        a window is dragged through the real shell input path
//
// Usage: bench_desktop_frame [--frames N] [--sdl] [--micro-only]
//   --micro-only  skip the desktop scenarios and only run the drawing-primitive micro-benchmarks
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
#include "../desktop/software_surface.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/theme.hpp"
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

// Per-call cost of individual drawing operations on surfaces the size of a typical window / the desktop.
// Toolkit cases use only the IWidgetToolkit API so numbers are comparable before and after toolkit changes.
void run_micro_benchmarks() {
    desktop::BasicWidgetToolkit toolkit;
    auto surface = std::make_shared<desktop::SoftwareSurface>(1920, 1080);
    auto window = std::make_shared<desktop::SoftwareSurface>(700, 500);
    const desktop::Color opaque{70, 80, 90, 255};
    const desktop::Color translucent{34, 199, 220, 128};

    auto time_ms = [](int iterations, auto&& fn) {
        fn(); // warm-up
        const auto t0 = Clock::now();
        for (int i = 0; i < iterations; ++i) fn();
        return ms_since(t0) / iterations;
    };
    auto row = [](const char* label, double ms) { std::printf("    %-52s %10.4f ms\n", label, ms); };

    std::printf("\nDrawing micro-benchmarks (mean time per operation)\n");
    row("toolkit.draw_rect 700x500 opaque", time_ms(100, [&] { toolkit.draw_rect(window, 0, 0, 700, 500, opaque); }));
    row("toolkit.draw_rect 700x500 alpha 50%", time_ms(100, [&] { toolkit.draw_rect(window, 0, 0, 700, 500, translucent); }));
    row("toolkit.draw_rect 1920x1080 opaque (wallpaper)", time_ms(30, [&] { toolkit.draw_rect(surface, 0, 0, 1920, 1080, opaque); }));
    row("toolkit.draw_rect 40x30 opaque x1000 (small widgets)", time_ms(20, [&] {
        for (int i = 0; i < 1000; ++i) toolkit.draw_rect(window, (i * 7) % 600, (i * 3) % 450, 40, 30, opaque);
    }));
    row("toolkit.draw_panel 700x500", time_ms(100, [&] { toolkit.draw_panel(window, 0, 0, 700, 500); }));
    row("toolkit.draw_text 32 chars x100", time_ms(20, [&] {
        for (int i = 0; i < 100; ++i) toolkit.draw_text(window, 10, 10 + (i % 400), "The quick brown fox jumps over it", opaque);
    }));
    row("toolkit.draw_button 120x40 x100", time_ms(20, [&] {
        for (int i = 0; i < 100; ++i) toolkit.draw_button(window, 20, 20 + (i % 400), 120, 40, "Click Me");
    }));

    // Painter primitives (new in Stage 1). Each case builds its own Painter, as widgets do.
    const desktop::CornerRadii r10 = desktop::CornerRadii::uniform(10.0f);
    row("painter.fill_rect 700x500 opaque", time_ms(100, [&] { desktop::Painter p(*window); p.fill_rect({0, 0, 700, 500}, opaque); }));
    row("painter.fill_rect 700x500 alpha 50%", time_ms(100, [&] { desktop::Painter p(*window); p.fill_rect({0, 0, 700, 500}, translucent); }));
    row("painter.fill_rect 1920x1080 opaque (wallpaper)", time_ms(30, [&] { desktop::Painter p(*surface); p.fill_rect({0, 0, 1920, 1080}, opaque); }));
    row("painter.fill_rounded_rect 700x500 r=10 opaque", time_ms(100, [&] { desktop::Painter p(*window); p.fill_rounded_rect({0, 0, 700, 500}, r10, opaque); }));
    row("painter.fill_rounded_rect 700x500 r=10 alpha 50%", time_ms(100, [&] { desktop::Painter p(*window); p.fill_rounded_rect({0, 0, 700, 500}, r10, translucent); }));
    row("painter.fill_rounded_rect 120x36 r=8 x100 (buttons)", time_ms(50, [&] {
        desktop::Painter p(*window);
        for (int i = 0; i < 100; ++i) p.fill_rounded_rect({20, 20 + (i % 400), 120, 36}, desktop::CornerRadii::uniform(8.0f), opaque);
    }));
    row("painter.stroke_rounded_rect 700x500 r=10 1px", time_ms(100, [&] { desktop::Painter p(*window); p.stroke_rounded_rect({0, 0, 700, 500}, r10, 1.0f, opaque); }));
    row("painter.fill_gradient 1920x1080 dithered", time_ms(20, [&] {
        desktop::Painter p(*surface);
        p.fill_gradient({0, 0, 1920, 1080}, desktop::LinearGradient{desktop::Color{18, 23, 28, 255}, desktop::Color{14, 18, 22, 255}, true, true});
    }));
    row("painter.fill_radial_gradient r=600 (wallpaper glow)", time_ms(10, [&] {
        desktop::Painter p(*surface);
        p.fill_radial_gradient(1400.0f, 300.0f, 600.0f, desktop::Color{34, 199, 220, 28}, desktop::Color{34, 199, 220, 0});
    }));
    row("painter.fill_circle r=7 x1000 (control dots)", time_ms(50, [&] {
        desktop::Painter p(*window);
        for (int i = 0; i < 1000; ++i) p.fill_circle(10.5f + static_cast<float>(i % 600), 10.5f + static_cast<float>(i % 400), 7.0f, opaque);
    }));
    row("painter.draw_line 100px x1000", time_ms(20, [&] {
        desktop::Painter p(*window);
        for (int i = 0; i < 1000; ++i) p.draw_line(10.0f, 10.5f + static_cast<float>(i % 400), 110.0f, 10.5f + static_cast<float>(i % 400), 1.0f, opaque);
    }));
    row("painter.draw_shadow 700x500 blur 28 (window)", time_ms(10, [&] {
        desktop::Painter p(*surface);
        p.draw_shadow({200, 200, 700, 500}, r10, desktop::default_theme().elevation.window_active);
    }));
    row("painter.draw_shadow 120x36 blur 8 x20 (controls)", time_ms(20, [&] {
        desktop::Painter p(*window);
        for (int i = 0; i < 20; ++i) p.draw_shadow({100, 50 + (i % 300), 120, 36}, desktop::CornerRadii::uniform(8.0f), desktop::default_theme().elevation.control);
    }));
}

} // namespace

int main(int argc, char** argv) {
    int frames = 600;
    bool use_sdl = false;
    bool micro_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--micro-only") == 0) micro_only = true;
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = std::atoi(argv[++i]);
        if (std::strcmp(argv[i], "--sdl") == 0) use_sdl = true;
    }

#ifdef NDEBUG
    std::printf("Build: NDEBUG (optimized)\n");
#else
    std::printf("Build: Debug\n");
#endif
    if (micro_only) {
        run_micro_benchmarks();
        return 0;
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
                size.width, size.height, frames, use_sdl ? "SDL2 (vsync)" : "mock");

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

    run_micro_benchmarks();
    shell.reset();
    std::printf("\nDone.\n");
    return 0;
}
