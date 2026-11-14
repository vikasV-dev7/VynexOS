// Desktop usability: the window placement policy (title bars stay reachable), all-or-nothing resizing (a rejected size
// leaves window and surfaces as they were), and notifications as Overlay windows (above application windows, receiving
// the pointer over their own pixels only).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../apps/application_host.hpp"
#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "vynexos/apps/application_registry.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/notification_service.hpp"
#include "vynexos/desktop/surface_limits.hpp"
#include "vynexos/hal/input_driver.hpp"

#include "test_check.hpp"
#include "window_interaction.hpp"

using namespace vynexos;
using namespace vynexos::desktop;
using vynex_test::Point;

namespace {

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

class CapturingDisplay final : public IDisplayAbstraction {
public:
    explicit CapturingDisplay(hal::DisplaySize size) : m_size(size) {}
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        return {};
    }
    void shutdown() override {}
    hal::DisplaySize logical_size() const override { return m_size; }
    hal::FrameBuffer last{0, 0, {}};

private:
    hal::DisplaySize m_size;
};

class ProbeApp final : public apps::IApplication {
public:
    int mouse_events{0};
    bool initialize() override { return true; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event& ev) override {
        if (ev.payload.type() == typeid(hal::MouseEvent)) ++mouse_events;
        return true;
    }
    void render(ISurface& s) override {
        auto px = s.map_pixels();
        for (size_t i = 0; i < px.size(); i += 4) px[i] = 10, px[i + 1] = 160, px[i + 2] = 60, px[i + 3] = 255;
        s.unmap_pixels();
    }
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "Probe"; }
};

struct Rig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display;
    std::shared_ptr<BasicCompositor> compositor;
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<apps::ApplicationRegistry> registry = std::make_shared<apps::ApplicationRegistry>();
    std::shared_ptr<apps::DesktopShell> shell;
    std::shared_ptr<apps::ApplicationHost> host;
    vynex_test::Pointer pointer{*bus};

    explicit Rig(hal::DisplaySize size = {1920, 1080}) {
        display = std::make_shared<CapturingDisplay>(size);
        compositor = std::make_shared<BasicCompositor>(display, logger);
        shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, std::make_shared<BasicWindowDecorator>(toolkit),
                                                     std::make_shared<BasicTaskbar>(toolkit), bus);
        host = std::make_shared<apps::ApplicationHost>(registry, wm, compositor, shell);
        shell->set_application_host(host.get());
        auto probe = [] { return std::make_unique<ProbeApp>(); };
        host->register_application({"Win", "Window", {300, 200, 400, 300}}, probe);
        host->register_application({"Off", "Opens off a small screen", {700, 600, 400, 300}}, probe);
        host->register_application({"Big", "Bigger than a small screen", {100, 100, 900, 700}}, probe);
        host->register_application({"Under", "Under the notifications", {1500, 880, 400, 300}}, probe);
        shell->launch();
        settle();
    }
    ~Rig() {
        host->close_all();
        shell->set_application_host(nullptr);
    }
    void frame() {
        shell->update_frame();
        CHECK(compositor->render_frame(wm->build_scene()).has_value());
    }
    void settle() {
        for (int i = 0; i < 3; ++i) frame();
    }
    uint32_t open(const char* id) {
        const auto w = host->open(id);
        CHECK(w.has_value());
        settle();
        return *w;
    }
    WindowGeometry client(uint32_t w) const { return *wm->get_window_geometry(w); }
    ProbeApp* probe(uint32_t w) const { return dynamic_cast<ProbeApp*>(host->application_in(w)); }

    /// A point on the window's title bar that is on the work area and belongs to the window (so it can be grabbed); the
    /// test fails if there is none.
    Point reachable_title_point(uint32_t w) const {
        const Rect d = wm->get_decorated_window_bounds(w);
        const auto work = wm->get_work_area();
        const int32_t y = d.y + 6 + 18; // middle of the title bar (below the 6 px band)
        CHECK(y >= work.y && y < work.y + static_cast<int32_t>(work.height));
        for (int32_t x = std::max(d.x, work.x); x < std::min<int64_t>(int64_t{d.x} + d.width, int64_t{work.x} + work.width); ++x) {
            if (wm->get_window_at(x, y) == std::optional<uint32_t>{w} && wm->get_decoration_hit_region(w, x, y) == DecorationHitRegion::TitleBar) return {x, y};
        }
        vynex_test::context() += " | no reachable title-bar point";
        CHECK(false);
        return {};
    }
    /// The policy's invariant: the whole title bar is inside the work area vertically, and enough of it horizontally.
    void expect_title_bar_reachable(uint32_t w) const {
        const Rect d = wm->get_decorated_window_bounds(w);
        const auto work = wm->get_work_area();
        const auto margins = wm->get_window_margins(w);
        CHECK(d.y >= work.y && d.y + margins.top <= work.y + static_cast<int32_t>(work.height));
        const int64_t visible = std::min<int64_t>(int64_t{d.x} + d.width, int64_t{work.x} + work.width) - std::max<int64_t>(d.x, work.x);
        CHECK(visible >= std::min<int64_t>({kMinVisibleTitleBar, d.width, work.width}));
        (void)reachable_title_point(w);
    }
    void notify(uint32_t id, const char* title) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = "notification_show";
        ev->payload = Notification{id, title, "body", NotificationPriority::Normal};
        bus->dispatch_immediate(ev);
    }
    void dismiss(uint32_t id) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = "notification_dismiss";
        ev->payload = id;
        bus->dispatch_immediate(ev);
    }
    std::vector<uint32_t> windows_titled(const std::string& title) const {
        std::vector<uint32_t> ids;
        for (const auto& s : wm->get_window_snapshots()) {
            if (s.title == title) ids.push_back(s.window_id);
        }
        return ids;
    }
    std::array<uint8_t, 3> screen(int32_t x, int32_t y) const {
        const size_t i = (static_cast<size_t>(y) * display->last.width + static_cast<size_t>(x)) * 4;
        return {display->last.pixels[i], display->last.pixels[i + 1], display->last.pixels[i + 2]};
    }
};

// ---- the placement policy itself ---------------------------------------------------------------------------------------
void test_placement_policy() {
    const WindowMargins m{42, 7, 7, 7}; // the decorator's normal frame: 414 x 349 decorated for a 400 x 300 client
    const WindowGeometry work{0, 0, 1920, 1040};
    auto at = [&](int32_t x, int32_t y, uint32_t w = 400, uint32_t h = 300, WindowGeometry area = {0, 0, 1920, 1040}) {
        return constrain_window_position({x, y, w, h}, m, area);
    };
    auto is = [](const WindowGeometry& g, int32_t x, int32_t y) { return g.x == x && g.y == y; };
    vynex_test::context() = "placement policy";
    CHECK(is(at(300, 200), 300, 200));                 // inside: unchanged
    CHECK(at(300, 200).width == 400 && at(300, 200).height == 300); // never resizes
    CHECK(is(at(-5000, 300), 128 - 414 + 7, 300));     // off the left: 128 px of the title bar (its right end) remain
    CHECK(is(at(5000, 300), 1920 - 128 + 7, 300));     // off the right: 128 px (its left end) remain
    CHECK(is(at(300, -5000), 300, 42));                // the title bar never goes above the top edge
    CHECK(is(at(300, 5000), 300, 1040));               // ...nor under the taskbar: its bottom is the work area's bottom
    CHECK(is(at(-5000, -5000), -279, 42) && is(at(5000, 5000), 1799, 1040));
    CHECK(is(at(-200, 300), -200, 300));               // partly off the left by choice: allowed (221 px remain)
    CHECK(is(at(300, 900), 300, 900));                 // body below the taskbar by choice: allowed (title bar visible)
    CHECK(is(at(-5000, 300, 50), 7, 300) && is(at(5000, 300, 50), 1920 - 64 + 7, 300)); // narrow window: kept whole
    // A work area smaller than the window, and one shorter than the title bar.
    CHECK(is(at(5000, 5000, 900, 700, {0, 0, 640, 440}), 640 - 128 + 7, 440));
    CHECK(is(at(-5000, 5000, 400, 300, {0, 0, 100, 30}), 100 - 414 + 7, 42)); // 100 px wide: all of it; title from the top
    // A work area that does not start at 0,0.
    CHECK(is(at(-5000, -5000, 400, 300, {100, 50, 800, 600}), 100 + 128 - 414 + 7, 50 + 42));
    // Extreme coordinates do not overflow.
    constexpr int32_t kMin = std::numeric_limits<int32_t>::min(), kMax = std::numeric_limits<int32_t>::max();
    CHECK(is(at(kMin, kMin), -279, 42) && is(at(kMax, kMax), 1799, 1040));
    (void)work;
    std::printf("test_placement_policy passed\n");
}

// ---- dragging -------------------------------------------------------------------------------------------------------------
void test_dragging_toward_every_edge() {
    Rig r;
    const uint32_t w = r.open("Win");
    const auto home = r.client(w);
    struct Target {
        const char* name;
        int32_t dx, dy; // pointer movement
    };
    const Target targets[] = {
        {"left", -4000, 0}, {"right", 4000, 0}, {"top", 0, -4000}, {"bottom", 0, 4000},
        {"top-left", -4000, -4000}, {"top-right", 4000, -4000}, {"bottom-left", -4000, 4000}, {"bottom-right", 4000, 4000},
    };
    for (const Target& t : targets) {
        vynex_test::context() = std::string("drag toward the ") + t.name;
        const auto before = r.client(w);
        const Point grab = r.reachable_title_point(w);
        r.pointer.drag(grab, {grab.x + t.dx, grab.y + t.dy});
        r.settle();
        const auto after = r.client(w);
        const auto expected = constrain_window_position({before.x + t.dx, before.y + t.dy, before.width, before.height}, r.wm->get_window_margins(w),
                                                        r.wm->get_work_area());
        CHECK(after.x == expected.x && after.y == expected.y && after.width == before.width && after.height == before.height);
        r.expect_title_bar_reachable(w);
        // ...and it can be dragged home again by what is left of its title bar.
        const Point back = r.reachable_title_point(w);
        r.pointer.drag(back, {back.x + (home.x - after.x), back.y + (home.y - after.y)});
        r.settle();
        CHECK(r.client(w).x == home.x && r.client(w).y == home.y);
    }
    std::printf("test_dragging_toward_every_edge passed (%zu directions)\n", std::size(targets));
}

void test_small_display() {
    Rig r({640, 480}); // work area 640 x 432 (the taskbar is 48 px)
    vynex_test::context() = "a window whose usual place is off a small screen opens with its title bar on screen";
    const uint32_t off = r.open("Off");
    r.expect_title_bar_reachable(off);
    vynex_test::context() = "a window bigger than the screen";
    const uint32_t big = r.open("Big");
    r.expect_title_bar_reachable(big);
    for (const Point d : {Point{-3000, 0}, Point{3000, 3000}, Point{0, -3000}}) {
        const Point grab = r.reachable_title_point(big);
        r.pointer.drag(grab, {grab.x + d.x, grab.y + d.y});
        r.settle();
        r.expect_title_bar_reachable(big);
    }
    vynex_test::context() = "maximized on a small screen";
    const Point grab = r.reachable_title_point(big);
    r.pointer.drag(grab, {grab.x - 3000, grab.y}); // pushed off the left: the controls are at the visible end
    r.settle();
    const Point maximize = vynex_test::control_point(*r.wm, big, DecorationHitRegion::MaximizeButton);
    CHECK(maximize.x >= 0 && maximize.x < 640 && maximize.y >= 0 && maximize.y < 432); // on screen, so a user can click it
    r.pointer.click(maximize);
    r.settle();
    const auto max = r.client(big);
    CHECK(max.x == 0 && max.width == 640 && max.y == 36 && max.height == 432 - 36);
    std::printf("test_small_display passed\n");
}

void test_maximize_restore_after_constrained_move_and_resizing() {
    Rig r;
    const uint32_t w = r.open("Win");
    vynex_test::context() = "constrained to the bottom-left corner";
    Point grab = r.reachable_title_point(w);
    r.pointer.drag(grab, {grab.x - 4000, grab.y + 4000});
    r.settle();
    const auto parked = r.client(w);
    r.expect_title_bar_reachable(w);
    vynex_test::context() = "its controls stay reachable (they are at the visible end of the title bar)";
    r.pointer.click(vynex_test::control_point(*r.wm, w, DecorationHitRegion::MaximizeButton));
    r.settle();
    CHECK(r.wm->get_window_state(w) == WindowState::Maximized);
    r.pointer.click(vynex_test::control_point(*r.wm, w, DecorationHitRegion::MaximizeButton));
    r.settle();
    CHECK(r.wm->get_window_state(w) == WindowState::Normal);
    const auto restored = r.client(w);
    CHECK(restored.x == parked.x && restored.y == parked.y && restored.width == parked.width && restored.height == parked.height);

    vynex_test::context() = "resizing the top edge stops at the work area's top; the bottom edge stays";
    grab = r.reachable_title_point(w);
    r.pointer.drag(grab, {grab.x + (300 - parked.x), grab.y + (300 - parked.y)}); // back to (300, 300)
    r.settle();
    const auto mid = r.client(w);
    const Point top = vynex_test::resize_point(*r.wm, w, ResizeEdge::Top);
    r.pointer.drag(top, {top.x, top.y - 2000});
    r.settle();
    const auto grown = r.client(w);
    CHECK(grown.y == r.wm->get_window_margins(w).top); // title bar at the top of the work area
    CHECK(int64_t{grown.y} + grown.height == int64_t{mid.y} + mid.height);
    r.expect_title_bar_reachable(w);
    vynex_test::context() = "other edges are not constrained: the right edge may go past the screen";
    const Point right = vynex_test::resize_point(*r.wm, w, ResizeEdge::Right);
    r.pointer.drag(right, {right.x + 3000, right.y});
    r.settle();
    const auto wide = r.client(w);
    CHECK(wide.x == grown.x && int64_t{wide.x} + wide.width > 1920);
    std::printf("test_maximize_restore_after_constrained_move_and_resizing passed\n");
}

// ---- rejected resizing ---------------------------------------------------------------------------------------------------
void test_rejected_resize_preserves_everything() {
    Rig r;
    const uint32_t w = r.open("Win");
    const auto m = r.wm->get_window_margins(w);
    const uint32_t extra_w = static_cast<uint32_t>(m.left + m.right), extra_h = static_cast<uint32_t>(m.top + m.bottom);
    auto layers = [&] {
        std::vector<RenderLayer> out;
        for (const auto& l : r.wm->build_scene()) {
            if (l.window_id == w) out.push_back(l);
        }
        CHECK(out.size() == 2); // decoration, content
        return out;
    };
    auto expect_consistent = [&] {
        // What is composited (layer bounds) is what is hit-tested (geometry) is what the surfaces hold.
        const auto g = r.client(w);
        const Rect d = r.wm->get_decorated_window_bounds(w);
        const auto l = layers();
        CHECK(l[0].bounds.x == d.x && l[0].bounds.width == d.width && l[0].bounds.height == d.height);
        CHECK(l[1].bounds.x == g.x && l[1].bounds.y == g.y && l[1].bounds.width == g.width && l[1].bounds.height == g.height);
        CHECK(l[0].surface->width() == d.width && l[0].surface->height() == d.height);
        CHECK(l[1].surface->width() == g.width && l[1].surface->height() == g.height);
        CHECK(r.wm->get_window_at(g.x + static_cast<int32_t>(g.width) - 1, g.y + 5) == std::optional<uint32_t>{w});
    };
    expect_consistent();
    const auto valid = r.client(w);
    const auto surfaces = layers();

    struct Case {
        const char* what;
        uint32_t width, height;
    };
    const Case rejected[] = {
        {"one pixel wider than the decorated limit", kMaxSurfaceDimension - extra_w + 1, 300},
        {"one pixel taller than the decorated limit", 400, kMaxSurfaceDimension - extra_h + 1},
        {"each side allowed, too many pixels", 9000, 9000},
        {"just past the pixel limit", 8192 - extra_w, 8192 - extra_h + 1},
        {"UINT32_MAX wide (would wrap if the margins were added in 32 bits)", std::numeric_limits<uint32_t>::max(), 300},
        {"UINT32_MAX by UINT32_MAX", std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()},
    };
    for (const Case& c : rejected) {
        vynex_test::context() = std::string("rejected: ") + c.what;
        CHECK(!r.wm->set_window_geometry(w, {50, 60, c.width, c.height}));
        CHECK(!r.wm->resize_window(w, c.width, c.height));
        r.settle();
        const auto now = r.client(w);
        CHECK(now.x == valid.x && now.y == valid.y && now.width == valid.width && now.height == valid.height); // nothing moved
        const auto l = layers();
        CHECK(l[0].surface == surfaces[0].surface && l[1].surface == surfaces[1].surface); // the same surfaces
        expect_consistent();
        // A point beyond the old bounds is not the window's.
        CHECK(r.wm->get_window_at(valid.x + static_cast<int32_t>(valid.width) + 40, valid.y + 5) != std::optional<uint32_t>{w});
    }

    vynex_test::context() = "boundary sizes are accepted (window manager level, no surfaces made)";
    {
        BasicWindowManager wm(r.bus, r.logger);
        const uint32_t id = wm.create_window("Edge", {0, 0, 100, 100});
        wm.set_window_frame_styles(id, BasicWindowDecorator(r.toolkit).frame_style(WindowState::Normal),
                                   BasicWindowDecorator(r.toolkit).frame_style(WindowState::Maximized));
        CHECK(wm.set_window_geometry(id, {0, 0, kMaxSurfaceDimension - extra_w, 100}));          // exactly 16384 decorated wide
        CHECK(wm.resize_window(id, 400, kMaxSurfaceDimension - extra_h));                         // exactly 16384 tall
        CHECK(wm.resize_window(id, 8192 - extra_w, 8192 - extra_h));                               // exactly the pixel limit
        CHECK(wm.resize_window(id, 0, 0) && wm.get_window_geometry(id)->width == 100 && wm.get_window_geometry(id)->height == 50); // raised to the minimum
    }

    vynex_test::context() = "recovery: a valid size afterwards applies normally";
    CHECK(r.wm->set_window_geometry(w, {250, 220, 520, 380}));
    r.settle();
    const auto l = layers();
    CHECK(l[1].surface != surfaces[1].surface && l[1].surface->width() == 520 && l[1].surface->height() == 380);
    expect_consistent();
    std::printf("test_rejected_resize_preserves_everything passed (%zu rejected sizes)\n", std::size(rejected));
}

// ---- notifications ---------------------------------------------------------------------------------------------------------
void test_notifications_are_overlays() {
    Rig r;
    const uint32_t under = r.open("Under"); // client 1500..1900 x 880..1180: under the notification area
    std::shared_ptr<ISurface> wallpaper;
    for (const auto& l : r.wm->build_scene()) {
        if (r.wm->get_window_layer(l.window_id) == WindowLayer::Desktop) wallpaper = l.surface;
    }
    CHECK(wallpaper != nullptr);
    const std::vector<uint8_t> wallpaper_before(wallpaper->map_pixels().begin(), wallpaper->map_pixels().end());
    wallpaper->unmap_pixels();

    vynex_test::context() = "a notification is an Overlay window exactly the size of its panel";
    r.notify(1, "First");
    r.settle();
    auto views = r.windows_titled("Notification");
    CHECK(views.size() == 1);
    const uint32_t first = views[0];
    CHECK(r.wm->get_window_layer(first) == WindowLayer::Overlay && r.wm->is_window_visible(first));
    const auto g = r.client(first);
    CHECK(g.x == 1600 && g.y == 967 && g.width == 300 && g.height == 50); // just above the 48 px taskbar, at the right
    int layers_of_first = 0;
    for (const auto& l : r.wm->build_scene()) layers_of_first += (l.window_id == first) ? 1 : 0;
    CHECK(layers_of_first == 1); // no window frame

    vynex_test::context() = "composed above the application window, and hit-tested the same way";
    const auto scene = r.wm->build_scene();
    CHECK(scene.back().window_id == first);
    const auto shown = r.screen(1650, 1000);
    CHECK(!(shown[0] == 10 && shown[1] == 160 && shown[2] == 60)); // not the window's colour
    auto px = scene.back().surface->map_pixels();
    const size_t s = (static_cast<size_t>(1000 - 967) * 300 + (1650 - 1600)) * 4;
    // The panel is the Overlay elevation: surface_overlay (#2A333D) inside a one-pixel border (#36414D), so it stands
    // out from the window bodies (#1A2027) it lies over.
    CHECK(px[0] == 0x36 && px[1] == 0x41 && px[2] == 0x4D && px[3] == 255);
    const size_t inside = (static_cast<size_t>(45) * 300 + 290) * 4; // clear of the two lines of text
    CHECK(px[inside] == 0x2A && px[inside + 1] == 0x33 && px[inside + 2] == 0x3D && px[inside + 3] == 255);
    const size_t last = (static_cast<size_t>(49) * 300 + 299) * 4;   // the border runs all the way round
    CHECK(px[last] == 0x36 && px[last + 1] == 0x41 && px[last + 2] == 0x4D && px[last + 3] == 255);
    CHECK(shown[0] == px[s] && shown[1] == px[s + 1] && shown[2] == px[s + 2]); // the notification's own pixel
    scene.back().surface->unmap_pixels();
    CHECK(r.wm->get_window_at(1650, 1000) == std::optional<uint32_t>{first});

    vynex_test::context() = "a press on a notification reaches nothing beneath and changes no focus";
    r.notify(2, "Second"); // stacks above the first, with a 10 px gap between them
    r.settle();
    views = r.windows_titled("Notification");
    CHECK(views.size() == 2);
    r.probe(under)->mouse_events = 0;
    const auto focus = r.shell->focused_window();
    r.pointer.click({1650, 1000});
    r.settle();
    CHECK(r.probe(under)->mouse_events == 0);
    CHECK(r.shell->focused_window() == focus);
    CHECK(r.wm->get_window_state(under) == WindowState::Normal && r.client(under).x == 1500); // nothing moved or resized

    vynex_test::context() = "outside the panels (the gap between them) the window beneath gets the pointer";
    CHECK(r.wm->get_window_at(1650, 962) == std::optional<uint32_t>{under}); // the panels are at 907..957 and 967..1017
    r.shell->clear_focus();
    r.pointer.click({1650, 962});
    r.settle();
    CHECK(r.probe(under)->mouse_events == 2);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{under});
    CHECK(r.wm->get_window_layer(under) == WindowLayer::Normal && r.windows_titled("Notification").size() == 2); // still below them
    CHECK(r.wm->build_scene().back().window_id != under);

    vynex_test::context() = "dismissed: the window beneath shows again; the wallpaper was never drawn on";
    r.dismiss(1);
    r.settle();
    views = r.windows_titled("Notification");
    CHECK(views.size() == 1 && r.client(views[0]).y == 967); // the remaining one moves down into the first slot
    r.dismiss(2);
    r.settle();
    CHECK(r.windows_titled("Notification").empty());
    const auto back = r.screen(1650, 1000);
    CHECK(back[0] == 10 && back[1] == 160 && back[2] == 60);
    CHECK(r.wm->get_window_at(1650, 1000) == std::optional<uint32_t>{under});
    const std::vector<uint8_t> wallpaper_after(wallpaper->map_pixels().begin(), wallpaper->map_pixels().end());
    wallpaper->unmap_pixels();
    CHECK(wallpaper_after == wallpaper_before);
    std::printf("test_notifications_are_overlays passed\n");
}

} // namespace

int main() {
    std::printf("Running desktop usability tests...\n");
    test_placement_policy();
    test_dragging_toward_every_edge();
    test_small_display();
    test_maximize_restore_after_constrained_move_and_resizing();
    test_rejected_resize_preserves_everything();
    test_notifications_are_overlays();
    std::printf("All desktop usability tests passed.\n");
    return 0;
}
