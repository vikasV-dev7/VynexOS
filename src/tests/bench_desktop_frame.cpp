// Desktop frame-cost benchmark (not a pass/fail test; intentionally not registered with CTest).
//
// Mirrors the CompositionRoot frame loop - shell/app update, scene build, compositor render - and
// reports per-phase timings for three workloads:
//   idle        nothing changes (steady state of the real loop)
//   full_dirty  the whole screen is invalidated every frame (worst case)
//   drag        a window is dragged through the real shell input path
//
// Usage: bench_desktop_frame [--frames N] [--sdl] [--micro-only]
//        bench_desktop_frame --dump <file.bmp> [--bitmap-text] [--focus-explorer] [--open-launcher] [--maximize-explorer]
//                            [--hover <minimize|maximize|close>] [--press <minimize|maximize|close>]
//   --hover/--press  point at (or press and hold) one of the Explorer's title-bar controls before capturing
//   --micro-only  skip the desktop scenarios and only run the drawing-primitive micro-benchmarks
//   --dump        render the desktop headlessly and write the compositor's exact output frame (1920x1080) as a BMP;
//                 --bitmap-text renders it with the legacy 8x8 font for a before/after comparison
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
#include "../apps/application_host.hpp"
#include "../apps/basic_terminal.hpp"
#include "../apps/demo_app.hpp"
#include "vynexos/apps/application_registry.hpp"
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
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/text_renderer.hpp"
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
    {
        // Window chrome: one full redraw of a 714x549 decoration surface (what a focus/size/title change costs).
        auto tk = std::make_shared<desktop::BasicWidgetToolkit>();
        desktop::BasicWindowDecorator decorator(tk);
        const desktop::FrameStyle style = decorator.frame_style(desktop::WindowState::Normal);
        auto deco = std::make_shared<desktop::SoftwareSurface>(714, 549);
        const auto layout = desktop::calculate_decoration_layout(desktop::make_frame_metrics(style, {0, 0, 714, 549}, {7, 42, 700, 500}));
        desktop::DecorationVisualState visual;
        visual.focused = true;
        row("decorator.draw_decorations 714x549 (full chrome)", time_ms(50, [&] { decorator.draw_decorations(deco, layout, "File Explorer", visual, desktop::WindowTheme{}); }));
        row("create_surface 714x549 (chrome surface alloc)", time_ms(50, [&] { desktop::SoftwareSurface s(714, 549); (void)s; }));
    }
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

    // Typography (Stage 2). draw_text now goes through the anti-aliased renderer; the 8x8 bitmap path is kept for comparison.
    const char* line = "The quick brown fox jumps over it";
    row("toolkit.draw_text_bitmap 32 chars x100 (legacy 8x8)", time_ms(20, [&] {
        for (int i = 0; i < 100; ++i) toolkit.draw_text_bitmap(window, 10, 10 + (i % 400), line, opaque);
    }));
    const desktop::TextRenderer* text = desktop::TextRenderer::shared_default();
    const desktop::TextStyle body = desktop::default_theme().type.body;
    if (text != nullptr) {
        row("TextRenderer::draw 32 chars x100 (body 14px)", time_ms(20, [&] {
            desktop::Painter p(*window);
            for (int i = 0; i < 100; ++i) text->draw(p, 10.0f, 24 + (i % 400), line, body, opaque);
        }));
        row("TextRenderer::draw 32 chars x100 (display 32px)", time_ms(20, [&] {
            desktop::Painter p(*window);
            for (int i = 0; i < 100; ++i) text->draw(p, 10.0f, 40 + (i % 400), "Settings", desktop::default_theme().type.display, opaque);
        }));
        float sink = 0.0f;
        row("TextRenderer::measure 32 chars x1000", time_ms(50, [&] {
            for (int i = 0; i < 1000; ++i) sink += text->measure(line, body).width;
        }));
        row("TextRenderer::draw_ellipsized 60 chars to 200px x100", time_ms(20, [&] {
            desktop::Painter p(*window);
            for (int i = 0; i < 100; ++i) text->draw_ellipsized(p, 10.0f, 24 + (i % 400), "A fairly long label that will not fit into the box", body, opaque, 200.0f);
        }));
        if (sink < 0.0f) std::printf("%f\n", static_cast<double>(sink)); // keep the loop observable
        row("TextRenderer::create (glyph atlas build, ONE-TIME at startup)", time_ms(5, [&] { auto r = desktop::TextRenderer::create(); (void)r; }));
        std::printf("    %-52s %10zu bytes\n", "glyph atlas coverage data", text->atlas_bytes());
    }
}

// Display that keeps the last composited frame, so the exact pixels the compositor produces can be written to disk.
// This is independent of any OS window, focus or GPU state, which makes it suitable for repeatable visual review.
class CapturingDisplay final : public desktop::IDisplayAbstraction {
public:
    std::expected<void, desktop::DisplayError> initialize() override { return {}; }
    std::expected<void, desktop::DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        return {};
    }
    void shutdown() override {}
    hal::FrameBuffer last{0, 0, {}};
};

// Writes a frame as an uncompressed 24-bit BMP (readable by every image viewer; no dependencies).
bool write_bmp(const char* path, const hal::FrameBuffer& fb) {
    if (fb.width == 0 || fb.height == 0 || fb.pixels.size() < static_cast<size_t>(fb.width) * fb.height * 4) return false;
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const uint32_t row_bytes = (fb.width * 3 + 3) & ~3u;
    const uint32_t image_bytes = row_bytes * fb.height;
    unsigned char header[54] = {'B', 'M'};
    auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) header[at + static_cast<size_t>(i)] = static_cast<unsigned char>(v >> (8 * i)); };
    put32(2, 54 + image_bytes);
    put32(10, 54);
    put32(14, 40);
    put32(18, fb.width);
    put32(22, fb.height);
    header[26] = 1;
    header[28] = 24;
    put32(34, image_bytes);
    std::fwrite(header, 1, sizeof header, f);
    std::vector<unsigned char> row(row_bytes, 0);
    for (uint32_t y = fb.height; y > 0; --y) { // BMP stores rows bottom-up
        const unsigned char* src = fb.pixels.data() + static_cast<size_t>(y - 1) * fb.width * 4;
        for (uint32_t x = 0; x < fb.width; ++x) {
            row[x * 3 + 0] = src[x * 4 + 2]; // B
            row[x * 3 + 1] = src[x * 4 + 1]; // G
            row[x * 3 + 2] = src[x * 4 + 0]; // R
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    return std::fclose(f) == 0;
}

} // namespace

int main(int argc, char** argv) {
    int frames = 600;
    bool use_sdl = false;
    bool micro_only = false;
    const char* dump_path = nullptr;  // --dump <file.bmp>: write one composited frame and exit
    bool bitmap_text = false;         // --bitmap-text: use the legacy 8x8 font (the pre-Stage-2 look) for a before/after
    bool focus_explorer = false;      // --focus-explorer: click the Explorer title bar first (active title bar)
    bool open_launcher = false;       // --open-launcher: click the Vynex button first
    bool maximize_explorer = false;   // --maximize-explorer: maximize and focus the Explorer window first
    const char* hover_control = nullptr;  // --hover <control>: rest the pointer on an Explorer title-bar control
    const char* press_control = nullptr;  // --press <control>: press and hold an Explorer title-bar control
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--micro-only") == 0) micro_only = true;
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = std::atoi(argv[++i]);
        if (std::strcmp(argv[i], "--sdl") == 0) use_sdl = true;
        if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) dump_path = argv[++i];
        if (std::strcmp(argv[i], "--bitmap-text") == 0) bitmap_text = true;
        if (std::strcmp(argv[i], "--focus-explorer") == 0) focus_explorer = true;
        if (std::strcmp(argv[i], "--open-launcher") == 0) open_launcher = true;
        if (std::strcmp(argv[i], "--maximize-explorer") == 0) maximize_explorer = true;
        if (std::strcmp(argv[i], "--hover") == 0 && i + 1 < argc) hover_control = argv[++i];
        if (std::strcmp(argv[i], "--press") == 0 && i + 1 < argc) press_control = argv[++i];
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
    auto capture = std::make_shared<CapturingDisplay>();
    if (dump_path != nullptr) {
        display = capture;
    } else if (use_sdl) {
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
    auto toolkit = bitmap_text ? std::make_shared<desktop::BasicWidgetToolkit>(nullptr) : std::make_shared<desktop::BasicWidgetToolkit>();
    auto decorator = std::make_shared<desktop::BasicWindowDecorator>(toolkit);
    auto taskbar = std::make_shared<desktop::BasicTaskbar>(toolkit);
    auto shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
    auto vfs = std::make_shared<core::MemoryFileSystem>();
    // Same population as CompositionRoot::initialize(), through the same application host.
    auto registry = std::make_shared<apps::ApplicationRegistry>();
    auto host = std::make_shared<apps::ApplicationHost>(registry, wm, compositor, shell);
    shell->set_application_host(host.get());
    host->register_application({"Terminal", "Terminal", {100, 100, 600, 400}}, [toolkit]() { return std::make_unique<apps::BasicTerminal>(toolkit); });
    host->register_application({"BasicFileExplorer", "File Explorer", {200, 200, 700, 500}},
                               [vfs, toolkit]() { return std::make_unique<apps::BasicFileExplorer>(vfs, toolkit); });
    host->register_application({"DemoApp", "Demo App", {400, 300, 300, 200}}, [toolkit]() { return std::make_unique<apps::DemoApp>(toolkit); });
    shell->launch();
    (void)host->open("Terminal");
    const uint32_t explorer_id = host->open("BasicFileExplorer").value_or(0);
    (void)host->open("DemoApp");

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

    if (dump_path != nullptr) {
        // Same real input path as a user: dispatch the clicks, then let a few frames settle before capturing.
        auto click = [&](int32_t x, int32_t y) {
            mouse(x, y, 1);
            mouse(x, y, 0);
            for (int i = 0; i < 5; ++i) run_frame(warm);
        };
        if (open_launcher) click(50, 1060);
        if (focus_explorer) click(500, 180);
        if (maximize_explorer) {
            wm->set_window_state(explorer_id, desktop::WindowState::Maximized);
            shell->focus_window(explorer_id);
            compositor->invalidate_region({0, 0, size.width, size.height});
        }
        if (hover_control != nullptr || press_control != nullptr) {
            for (int i = 0; i < 5; ++i) run_frame(warm);
            const auto bounds = wm->get_decorated_window_bounds(explorer_id);
            const auto geom = wm->get_window_geometry(explorer_id);
            const auto layout = desktop::calculate_decoration_layout(desktop::make_frame_metrics(
                wm->get_window_frame_style(explorer_id), bounds, {geom->x, geom->y, geom->width, geom->height}));
            auto control_centre = [&](const char* name) {
                const desktop::Rect r = std::strcmp(name, "close") == 0      ? layout.close_button
                                        : std::strcmp(name, "maximize") == 0 ? layout.maximize_button
                                                                             : layout.minimize_button;
                return std::pair<int32_t, int32_t>{bounds.x + r.x + static_cast<int32_t>(r.width / 2), bounds.y + r.y + static_cast<int32_t>(r.height / 2)};
            };
            const auto [x, y] = control_centre(press_control != nullptr ? press_control : hover_control);
            mouse(x, y, 0);
            if (press_control != nullptr) mouse(x, y, 1); // held: the dump shows the pressed state
        }
        for (int i = 0; i < 10; ++i) run_frame(warm);
        if (!write_bmp(dump_path, capture->last)) {
            std::fprintf(stderr, "failed to write %s\n", dump_path);
            return 1;
        }
        std::printf("Wrote %ux%u frame (%s text) to %s\n", capture->last.width, capture->last.height, bitmap_text ? "legacy 8x8" : "Inter", dump_path);
        return 0;
    }

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

    // 4. hover: the pointer visits a different title-bar target every frame (worst case for hover feedback)
    {
        const auto bounds = wm->get_decorated_window_bounds(explorer_id);
        const auto geom = wm->get_window_geometry(explorer_id);
        const auto layout = desktop::calculate_decoration_layout(desktop::make_frame_metrics(
            wm->get_window_frame_style(explorer_id), bounds, {geom->x, geom->y, geom->width, geom->height}));
        auto centre = [&](const desktop::Rect& r) {
            return std::pair<int32_t, int32_t>{bounds.x + r.x + static_cast<int32_t>(r.width / 2), bounds.y + r.y + static_cast<int32_t>(r.height / 2)};
        };
        const std::pair<int32_t, int32_t> targets[] = {centre(layout.minimize_button), centre(layout.maximize_button),
                                                       centre(layout.close_button),
                                                       {bounds.x + layout.title_bar.x + 200, bounds.y + layout.title_bar.y + 18}};
        Phases p;
        for (int i = 0; i < frames; ++i) {
            const auto& t = targets[static_cast<size_t>(i) % std::size(targets)];
            mouse(t.first, t.second, 0);
            run_frame(p);
        }
        report("hover", p, "pointer moves between the Explorer's title-bar controls every frame");
    }

    run_micro_benchmarks();
    host->close_all();
    shell->set_application_host(nullptr);
    shell.reset();
    std::printf("\nDone.\n");
    return 0;
}
