// The shell's side of IApplication::take_invalidated_region(): an application is rendered when it reports a change (or
// has not been painted yet) and never otherwise; exactly the reported regions, clamped to the content area and moved to
// the window's place on screen, are recomposited; reports are drained every frame; misbehaving applications cannot stall
// a frame. A scripted application paints its whole surface in one colour and reports whatever the test tells it to.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "vynexos/apps/application.hpp"
#include "vynexos/core/logger.hpp"

#include "test_check.hpp"

using namespace vynexos;
using namespace vynexos::desktop;

namespace {

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

class CapturingDisplay final : public IDisplayAbstraction {
public:
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        return {};
    }
    void shutdown() override {}
    hal::FrameBuffer last{0, 0, {}};
};

class RecordingCompositor final : public ICompositor {
public:
    explicit RecordingCompositor(std::shared_ptr<BasicCompositor> inner) : m_inner(std::move(inner)) {}
    std::shared_ptr<ISurface> create_surface(uint32_t w, uint32_t h) override { return m_inner->create_surface(w, h); }
    void invalidate_region(const Rect& r) override {
        invalidated.push_back(r);
        m_inner->invalidate_region(r);
    }
    std::expected<void, DisplayError> render_frame(const SceneGraph& scene) override { return m_inner->render_frame(scene); }
    hal::DisplaySize logical_size() const override { return m_inner->logical_size(); }
    std::vector<Rect> invalidated;

private:
    std::shared_ptr<BasicCompositor> m_inner;
};

/// Paints its whole surface in `colour`; reports the regions queued in `pending`.
class ScriptedApp final : public apps::IApplication {
public:
    std::deque<Rect> pending;
    bool always_report{false}; // misbehaving: never stops reporting
    int renders{0};
    int reports_taken{0};
    uint8_t colour{40};

    bool initialize() override { return true; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event&) override { return false; }
    void render(ISurface& s) override {
        ++renders;
        auto px = s.map_pixels();
        for (size_t i = 0; i < px.size(); i += 4) {
            px[i] = colour;
            px[i + 1] = static_cast<uint8_t>(colour / 2);
            px[i + 2] = 200;
            px[i + 3] = 255;
        }
        s.unmap_pixels();
    }
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "ScriptedApp"; }
    std::optional<Rect> take_invalidated_region() override {
        if (always_report) {
            ++reports_taken;
            return Rect{0, 0, 1, 1};
        }
        if (pending.empty()) return std::nullopt;
        ++reports_taken;
        const Rect r = pending.front();
        pending.pop_front();
        return r;
    }
};

/// An application that relies on the interface default (reports nothing).
class StaticApp final : public apps::IApplication {
public:
    int renders{0};
    bool initialize() override { return true; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event&) override { return false; }
    void render(ISurface&) override { ++renders; }
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "StaticApp"; }
};

bool same(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }

struct Rig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>();
    std::shared_ptr<RecordingCompositor> compositor = std::make_shared<RecordingCompositor>(std::make_shared<BasicCompositor>(display, logger));
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<BasicWindowDecorator> decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    std::shared_ptr<BasicTaskbar> taskbar = std::make_shared<BasicTaskbar>(toolkit);
    std::unique_ptr<apps::DesktopShell> shell;

    Rig() {
        shell = std::make_unique<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        shell->launch();
        frame();
    }
    /// Hosts `app` in a window whose client area is `geom`, with a surface of `sw` x `sh`.
    uint32_t host(apps::IApplication* app, WindowGeometry geom, uint32_t sw, uint32_t sh, bool visible = true) {
        const uint32_t id = wm->create_window("App", geom);
        auto surface = compositor->create_surface(sw, sh);
        wm->set_window_surface(id, surface);
        RunningApplication ra;
        ra.app = app;
        ra.window_id = id;
        ra.surface = surface;
        ra.visible = visible;
        shell->add_running_application(std::move(ra));
        return id;
    }
    void frame() {
        shell->update_frame();
        const auto rendered = compositor->render_frame(wm->build_scene());
        CHECK(rendered.has_value());
    }
    /// The non-pointer, non-chrome redraw requests made since the last clear that fall inside `area`.
    std::vector<Rect> content_invalidations(const Rect& area) const {
        std::vector<Rect> out;
        for (const Rect& r : compositor->invalidated) {
            const bool within = r.x >= area.x && r.y >= area.y && static_cast<int64_t>(r.x) + r.width <= static_cast<int64_t>(area.x) + area.width &&
                                static_cast<int64_t>(r.y) + r.height <= static_cast<int64_t>(area.y) + area.height;
            if (within) out.push_back(r);
        }
        return out;
    }
    uint8_t red_at(int32_t x, int32_t y) const { return display->last.pixels[(static_cast<size_t>(y) * display->last.width + static_cast<size_t>(x)) * 4]; }
};

// ---------------------------------------------------------------------------------------------------------------------
void test_first_paint_then_nothing_until_reported() {
    Rig r;
    ScriptedApp app;
    const WindowGeometry g{300, 200, 200, 100};
    r.compositor->invalidated.clear();
    r.host(&app, g, 200, 100);
    r.frame();
    vynex_test::context() = "first frame";
    CHECK(app.renders == 1);
    const auto first = r.content_invalidations({g.x, g.y, g.width, g.height});
    CHECK(std::any_of(first.begin(), first.end(), [&](const Rect& x) { return same(x, Rect{300, 200, 200, 100}); })); // whole content
    CHECK(r.red_at(300 + 100, 200 + 50) == 40);

    vynex_test::context() = "idle frames";
    r.compositor->invalidated.clear();
    for (int i = 0; i < 20; ++i) r.frame();
    CHECK(app.renders == 1);                      // no change reported: not rendered again...
    CHECK(r.compositor->invalidated.empty());     // ...and nothing redrawn
    std::printf("test_first_paint_then_nothing_until_reported passed\n");
}

void test_exactly_the_reported_regions_are_recomposited() {
    Rig r;
    ScriptedApp app;
    const WindowGeometry g{300, 200, 200, 100};
    r.host(&app, g, 200, 100);
    r.frame();

    // The application changes everywhere but reports only one rectangle: only that rectangle may reach the screen,
    // proving the redraw is driven by the report and limited to it.
    app.colour = 120;
    app.pending.push_back({20, 10, 50, 30});
    r.compositor->invalidated.clear();
    r.frame();
    vynex_test::context() = "one reported region";
    CHECK(app.renders == 2);
    CHECK(r.compositor->invalidated.size() == 1);
    CHECK(same(r.compositor->invalidated[0], Rect{320, 210, 50, 30})); // moved to the window's place on screen
    CHECK(r.red_at(320, 210) == 120 && r.red_at(369, 239) == 120);     // inside: new
    CHECK(r.red_at(319, 210) == 40 && r.red_at(370, 239) == 40);       // just outside: untouched
    CHECK(r.red_at(320, 209) == 40 && r.red_at(320, 240) == 40);

    // Several regions in one frame: drained completely, one render, one redraw request each.
    app.colour = 160;
    app.pending = {{0, 0, 10, 10}, {100, 50, 10, 10}, {190, 90, 10, 10}};
    r.compositor->invalidated.clear();
    r.frame();
    vynex_test::context() = "three regions";
    CHECK(app.renders == 3);
    CHECK(app.pending.empty());
    CHECK(r.compositor->invalidated.size() == 3);
    CHECK(same(r.compositor->invalidated[0], Rect{300, 200, 10, 10}));
    CHECK(same(r.compositor->invalidated[1], Rect{400, 250, 10, 10}));
    CHECK(same(r.compositor->invalidated[2], Rect{490, 290, 10, 10}));

    // kWholeContent: the whole client area.
    app.pending.push_back(apps::kWholeContent);
    r.compositor->invalidated.clear();
    r.frame();
    vynex_test::context() = "whole content";
    CHECK(r.compositor->invalidated.size() == 1);
    CHECK(same(r.compositor->invalidated[0], Rect{300, 200, 200, 100}));
    CHECK(r.red_at(300, 200) == 160 && r.red_at(499, 299) == 160);
    std::printf("test_exactly_the_reported_regions_are_recomposited passed\n");
}

void test_regions_are_clamped_to_the_content() {
    Rig r;
    ScriptedApp app;
    // Hosted with a surface (120 x 60) smaller than the client area (200 x 100): the shell replaces it with one of the
    // client area's size (an application's surface is always exactly its client area), so content ends where the client
    // area does.
    const WindowGeometry g{300, 200, 200, 100};
    const uint32_t id = r.host(&app, g, 120, 60);
    r.frame();
    std::shared_ptr<ISurface> shown;
    for (const auto& layer : r.wm->build_scene()) {
        if (layer.window_id == id) shown = layer.surface;
    }
    vynex_test::context() = "surface fitted to the client area";
    CHECK(shown && shown->width() == 200 && shown->height() == 100);
    struct Case {
        const char* what;
        Rect reported;
        std::optional<Rect> expected;
    };
    const Case cases[] = {
        {"reaching past the top-left", {-10, -10, 30, 30}, Rect{300, 200, 20, 20}},
        {"reaching past the bottom-right", {190, 90, 100, 100}, Rect{490, 290, 10, 10}},
        {"whole content = the client area", apps::kWholeContent, Rect{300, 200, 200, 100}},
        {"entirely outside the content", {500, 500, 10, 10}, std::nullopt},
        {"empty", {10, 10, 0, 0}, std::nullopt},
    };
    for (const Case& c : cases) {
        vynex_test::context() = std::string("clamping: ") + c.what;
        app.pending.push_back(c.reported);
        r.compositor->invalidated.clear();
        r.frame();
        if (c.expected) {
            CHECK(r.compositor->invalidated.size() == 1);
            CHECK(same(r.compositor->invalidated[0], *c.expected));
        } else {
            CHECK(r.compositor->invalidated.empty());
        }
    }
    std::printf("test_regions_are_clamped_to_the_content passed\n");
}

void test_window_position_partly_off_screen() {
    Rig r;
    ScriptedApp app;
    const WindowGeometry g{-50, -20, 200, 100}; // partly off the top-left of the screen
    r.host(&app, g, 200, 100);
    r.frame();
    app.colour = 90;
    app.pending.push_back(apps::kWholeContent);
    r.frame();
    vynex_test::context() = "partly off-screen window";
    CHECK(r.red_at(0, 0) == 90 && r.red_at(149, 79) == 90); // the visible part shows the new content
    std::printf("test_window_position_partly_off_screen passed\n");
}

void test_misbehaving_and_static_and_hidden_applications() {
    Rig r;
    // Never stops reporting: the frame still completes, with one render and the whole content redrawn.
    ScriptedApp noisy;
    noisy.always_report = true;
    const WindowGeometry g{300, 200, 200, 100};
    r.host(&noisy, g, 200, 100);
    r.compositor->invalidated.clear();
    r.frame();
    vynex_test::context() = "application that never stops reporting";
    CHECK(noisy.renders == 1);
    CHECK(noisy.reports_taken == 64); // bounded drain
    CHECK(std::any_of(r.compositor->invalidated.begin(), r.compositor->invalidated.end(), [](const Rect& x) { return same(x, Rect{300, 200, 200, 100}); }));

    // Uses the interface default (never reports): painted once when first shown, then left alone.
    StaticApp still;
    r.host(&still, {600, 200, 100, 100}, 100, 100);
    for (int i = 0; i < 10; ++i) r.frame();
    vynex_test::context() = "application using the default (reports nothing)";
    CHECK(still.renders == 1);

    // Hidden: neither rendered nor drained (its report waits until it is visible).
    ScriptedApp hidden;
    hidden.pending.push_back({0, 0, 5, 5});
    r.host(&hidden, {800, 200, 100, 100}, 100, 100, /*visible=*/false);
    for (int i = 0; i < 5; ++i) r.frame();
    vynex_test::context() = "hidden application";
    CHECK(hidden.renders == 0);
    CHECK(hidden.pending.size() == 1);
    std::printf("test_misbehaving_and_static_and_hidden_applications passed\n");
}

void test_removed_application_is_forgotten() {
    Rig r;
    const uint32_t id = r.wm->create_window("App", {300, 200, 200, 100});
    auto surface = r.compositor->create_surface(200, 100);
    r.wm->set_window_surface(id, surface);
    auto host_on_same_surface = [&](apps::IApplication* app) {
        RunningApplication ra;
        ra.app = app;
        ra.window_id = id;
        ra.surface = surface; // the very same surface, so only the removal can make the shell paint again
        ra.visible = true;
        r.shell->add_running_application(std::move(ra));
    };
    ScriptedApp first;
    host_on_same_surface(&first);
    r.frame();
    CHECK(first.renders == 1);
    r.shell->remove_running_application(id);
    // A new application on the same window and surface starts unpainted again (its first frame paints it).
    ScriptedApp second;
    host_on_same_surface(&second);
    r.frame();
    vynex_test::context() = "application replaced on the same window and surface";
    CHECK(second.renders == 1);
    std::printf("test_removed_application_is_forgotten passed\n");
}

} // namespace

int main() {
    std::printf("Running application invalidation tests...\n");
    test_first_paint_then_nothing_until_reported();
    test_exactly_the_reported_regions_are_recomposited();
    test_regions_are_clamped_to_the_content();
    test_window_position_partly_off_screen();
    test_misbehaving_and_static_and_hidden_applications();
    test_removed_application_is_forgotten();
    std::printf("All application invalidation tests passed.\n");
    return 0;
}
