// The geometry contract between the window manager, decorated windows, application content and input routing (see
// window_manager.hpp), checked through the real desktop shell, window manager and compositor:
//   - pointer input reaches an application in content coordinates (desktop point minus the client origin), only over its
//     client area or while a press that began there is held, and never for presses on decorations or elsewhere;
//   - an application's surface is always exactly its client area: it follows resizing from every edge, maximize,
//     restore and minimum sizes, is replaced only when the size changes, and the old one is released;
//   - the application instance and its state survive all of it, and the screen shows the content at its new size.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
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
#include "vynexos/hal/viewport.hpp"

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
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        return {};
    }
    void shutdown() override {}
    hal::FrameBuffer last{0, 0, {}};
};

/// Forwards to the real compositor and records the size of every surface it is asked for.
class RecordingCompositor final : public ICompositor {
public:
    explicit RecordingCompositor(std::shared_ptr<BasicCompositor> inner) : m_inner(std::move(inner)) {}
    std::shared_ptr<ISurface> create_surface(uint32_t w, uint32_t h) override {
        created.push_back({w, h});
        return m_inner->create_surface(w, h);
    }
    void invalidate_region(const Rect& r) override { m_inner->invalidate_region(r); }
    std::expected<void, DisplayError> render_frame(const SceneGraph& scene) override { return m_inner->render_frame(scene); }
    hal::DisplaySize logical_size() const override { return m_inner->logical_size(); }
    struct Size {
        uint32_t w, h;
    };
    std::vector<Size> created;
    int created_of(uint32_t w, uint32_t h) const {
        return static_cast<int>(std::count_if(created.begin(), created.end(), [&](const Size& s) { return s.w == w && s.h == h; }));
    }

private:
    std::shared_ptr<BasicCompositor> m_inner;
};

/// Records the pointer events it receives (as delivered: content coordinates) and paints a position-dependent pattern,
/// so content that is misplaced, stretched or stale on screen cannot match its surface.
class GeometryApp final : public apps::IApplication {
public:
    struct Received {
        int32_t x, y;
        uint8_t buttons;
    };
    std::vector<Received> events;
    int initializations{0};
    int renders{0};
    uint32_t rendered_w{0}, rendered_h{0};
    int state{0}; // stands for application state that must survive resizing
    uint8_t generation{1};

    bool initialize() override {
        ++initializations;
        return true;
    }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event& ev) override {
        if (ev.topic != hal::kTopicInputMouse || ev.payload.type() != typeid(hal::MouseEvent)) return false;
        const auto m = std::any_cast<hal::MouseEvent>(ev.payload);
        events.push_back({m.x, m.y, m.button_state});
        return true;
    }
    void render(ISurface& s) override {
        ++renders;
        rendered_w = s.width();
        rendered_h = s.height();
        auto px = s.map_pixels();
        for (uint32_t y = 0; y < s.height(); ++y) {
            for (uint32_t x = 0; x < s.width(); ++x) {
                const size_t i = (static_cast<size_t>(y) * s.width() + x) * 4;
                px[i] = static_cast<uint8_t>(x * 7 + y * 13 + generation);
                px[i + 1] = static_cast<uint8_t>(y);
                px[i + 2] = static_cast<uint8_t>(x);
                px[i + 3] = 255;
            }
        }
        s.unmap_pixels();
    }
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "GeometryApp"; }
};

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
    vynex_test::Pointer pointer{*bus};
    GeometryApp app;
    uint32_t win{0};

    /// Hosts the application the way CompositionRoot does: a window, a surface of the window's size, the shell.
    explicit Rig(WindowGeometry geom = {300, 200, 400, 300}) {
        shell = std::make_unique<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        shell->launch();
        CHECK(app.initialize());
        win = wm->create_window("App", geom);
        auto surface = compositor->create_surface(geom.width, geom.height);
        wm->set_window_surface(win, surface);
        RunningApplication ra;
        ra.app = &app;
        ra.window_id = win;
        ra.surface = std::move(surface);
        shell->add_running_application(std::move(ra));
        shell->focus_window(win);
        settle();
    }
    void frame() {
        shell->update_frame();
        CHECK(compositor->render_frame(wm->build_scene()).has_value());
    }
    void settle() {
        for (int i = 0; i < 3; ++i) frame();
    }
    WindowGeometry client() const { return *wm->get_window_geometry(win); }
    Point at(int32_t local_x, int32_t local_y) const { return {client().x + local_x, client().y + local_y}; }

    /// The surface the compositor shows for the window's content (the last layer of the window in the scene).
    std::shared_ptr<ISurface> content_surface() const {
        std::shared_ptr<ISurface> s;
        for (const auto& layer : wm->build_scene()) {
            if (layer.window_id == win) s = layer.surface;
        }
        return s;
    }

    /// Click at a content point and return what the application received.
    std::vector<GeometryApp::Received> click_content(int32_t local_x, int32_t local_y) {
        app.events.clear();
        pointer.click(at(local_x, local_y));
        settle();
        return app.events;
    }

    /// The application's surface is the client area, the compositor shows that very surface, and the screen over the
    /// client area equals it byte for byte (no stale, stretched or misplaced content).
    void expect_content_matches_client() {
        const auto c = client();
        const auto shown = content_surface();
        const auto* running = shell->focused_application();
        CHECK(shown != nullptr);
        CHECK(shown->width() == c.width && shown->height() == c.height);
        if (running && running->window_id == win) CHECK(running->surface == shown);
        CHECK(app.rendered_w == c.width && app.rendered_h == c.height);

        const auto& fb = display->last;
        auto px = shown->map_pixels();
        size_t mismatch = 0;
        for (uint32_t y = 0; y < c.height; ++y) {
            const int64_t sy = static_cast<int64_t>(c.y) + y;
            if (sy < 0 || sy >= static_cast<int64_t>(fb.height)) continue;
            for (uint32_t x = 0; x < c.width; ++x) {
                const int64_t sx = static_cast<int64_t>(c.x) + x;
                if (sx < 0 || sx >= static_cast<int64_t>(fb.width)) continue;
                const size_t s = (static_cast<size_t>(y) * c.width + x) * 4;
                const size_t f = (static_cast<size_t>(sy) * fb.width + static_cast<size_t>(sx)) * 4;
                if (px[s] != fb.pixels[f] || px[s + 1] != fb.pixels[f + 1] || px[s + 2] != fb.pixels[f + 2]) ++mismatch;
            }
        }
        shown->unmap_pixels();
        if (mismatch) context() += " | " + std::to_string(mismatch) + " composed pixels differ from the surface";
        CHECK(mismatch == 0);
    }
    static std::string& context() { return vynex_test::context(); }

    /// Pixels inside `old_bounds` but outside the window's current decorated bounds show the wallpaper again.
    void expect_vacated_area_is_wallpaper(const Rect& old_bounds) {
        const Rect now = wm->get_decorated_window_bounds(win);
        const auto& fb = display->last;
        int stale = 0;
        for (int64_t y = std::max<int64_t>(old_bounds.y, 0); y < std::min<int64_t>(static_cast<int64_t>(old_bounds.y) + old_bounds.height, fb.height); ++y) {
            for (int64_t x = std::max<int64_t>(old_bounds.x, 0); x < std::min<int64_t>(static_cast<int64_t>(old_bounds.x) + old_bounds.width, fb.width); ++x) {
                const bool covered = x >= now.x && y >= now.y && x < static_cast<int64_t>(now.x) + now.width && y < static_cast<int64_t>(now.y) + now.height;
                if (covered) continue;
                const size_t i = (static_cast<size_t>(y) * fb.width + static_cast<size_t>(x)) * 4;
                if (!(fb.pixels[i] == 30 && fb.pixels[i + 1] == 30 && fb.pixels[i + 2] == 30)) ++stale;
            }
        }
        if (stale) context() += " | " + std::to_string(stale) + " stale pixels where the window used to be";
        CHECK(stale == 0);
    }
};

bool received_exactly(const std::vector<GeometryApp::Received>& got, std::initializer_list<GeometryApp::Received> want) {
    if (got.size() != want.size()) {
        vynex_test::context() += " | received " + std::to_string(got.size()) + " events, expected " + std::to_string(want.size());
        return false;
    }
    size_t i = 0;
    for (const auto& w : want) {
        const auto& g = got[i++];
        if (g.x != w.x || g.y != w.y || g.buttons != w.buttons) {
            vynex_test::context() += " | event " + std::to_string(i - 1) + " was (" + std::to_string(g.x) + "," + std::to_string(g.y) + ",b" +
                                     std::to_string(g.buttons) + "), expected (" + std::to_string(w.x) + "," + std::to_string(w.y) + ",b" +
                                     std::to_string(w.buttons) + ")";
            return false;
        }
    }
    return true;
}

// ---- P-3: pointer coordinates ---------------------------------------------------------------------------------------

void test_clicks_arrive_in_content_coordinates() {
    Rig r;
    const auto c = r.client();
    struct P {
        int32_t x, y;
    };
    // The client's top-left corner, its neighbours, its last column and row, and points in between. (The bottom corners
    // are covered by the window manager's corner resize grips: see test_decorations_are_not_application_input.)
    const int32_t last_x = static_cast<int32_t>(c.width) - 1, last_y = static_cast<int32_t>(c.height) - 1;
    for (const P p : {P{0, 0}, P{1, 0}, P{0, 1}, P{last_x, 0}, P{last_x, 150}, P{200, last_y}, P{0, 150}, P{200, 150}, P{17, 233}}) {
        r.context() = "click at content (" + std::to_string(p.x) + "," + std::to_string(p.y) + ")";
        CHECK(received_exactly(r.click_content(p.x, p.y), {{p.x, p.y, 1}, {p.x, p.y, 0}}));
    }
    // Free pointer motion over the content arrives too, in content coordinates.
    r.app.events.clear();
    r.pointer.send(r.at(123, 45), 0);
    r.context() = "hover over the content";
    CHECK(received_exactly(r.app.events, {{123, 45, 0}}));
    std::printf("test_clicks_arrive_in_content_coordinates passed\n");
}

void test_window_positions_and_frame_sizes() {
    struct Case {
        const char* what;
        WindowGeometry geom;
    };
    const Case cases[] = {
        {"near the top-left", {20, 50, 200, 120}},
        {"lower right, large", {1100, 500, 640, 400}},
        {"small", {700, 300, 120, 80}},
        {"partly off the left edge", {-60, 400, 300, 200}},
    };
    for (const Case& k : cases) {
        Rig r(k.geom);
        r.context() = std::string(k.what) + ": top-left corner";
        const int32_t x0 = k.geom.x < 0 ? -k.geom.x : 0; // first content column that is on screen
        CHECK(received_exactly(r.click_content(x0, 0), {{x0, 0, 1}, {x0, 0, 0}}));
        r.context() = std::string(k.what) + ": last column and last row";
        const int32_t fx = static_cast<int32_t>(k.geom.width) - 1, fy = static_cast<int32_t>(k.geom.height) - 1;
        CHECK(received_exactly(r.click_content(fx, fy / 2), {{fx, fy / 2, 1}, {fx, fy / 2, 0}}));
        CHECK(received_exactly(r.click_content(fx / 2, fy), {{fx / 2, fy, 1}, {fx / 2, fy, 0}}));
    }

    // A maximized window has different margins (a title bar, no borders): content coordinates still start at 0,0.
    Rig r;
    r.pointer.click(vynex_test::control_point(*r.wm, r.win, DecorationHitRegion::MaximizeButton));
    r.settle();
    CHECK(r.wm->get_window_state(r.win) == WindowState::Maximized);
    const auto m = r.wm->get_window_margins(r.win);
    CHECK(m.left == 0 && m.top > 0);
    r.context() = "maximized: top-left corner";
    CHECK(received_exactly(r.click_content(0, 0), {{0, 0, 1}, {0, 0, 0}}));
    r.context() = "maximized: inside";
    CHECK(received_exactly(r.click_content(640, 333), {{640, 333, 1}, {640, 333, 0}}));
    std::printf("test_window_positions_and_frame_sizes passed\n");
}

void test_moving_the_window() {
    Rig r;
    const auto c0 = r.client();
    const Point grab = vynex_test::title_bar_point(*r.wm, r.win);
    r.app.events.clear();
    r.pointer.drag(grab, {grab.x - 170, grab.y + 260});
    r.settle();
    r.context() = "window moved by its title bar";
    CHECK(r.client().x == c0.x - 170 && r.client().y == c0.y + 260);
    CHECK(r.app.events.empty()); // moving the window is not application input
    CHECK(received_exactly(r.click_content(0, 0), {{0, 0, 1}, {0, 0, 0}}));
    CHECK(received_exactly(r.click_content(250, 100), {{250, 100, 1}, {250, 100, 0}}));
    r.expect_content_matches_client();
    std::printf("test_moving_the_window passed\n");
}

void test_viewport_scaling() {
    // Host windows of other sizes than the logical desktop: the HAL maps host pixels to desktop coordinates (scale and
    // letterbox); the shell must apply nothing more than the client origin on top of that.
    Rig r;
    const auto c = r.client();
    const auto logical = r.compositor->logical_size();
    struct Host {
        uint32_t w, h;
    };
    for (const Host host : {Host{960, 540}, Host{1280, 1024}, Host{3840, 2160}, Host{1000, 1000}}) {
        const hal::Viewport vp = hal::compute_viewport(host.w, host.h, logical.width, logical.height);
        CHECK(vp.width > 0 && vp.height > 0);
        for (const Point target : {Point{0, 0}, Point{150, 90}, Point{399, 150}, Point{200, 299}}) {
            // The host pixel over the desktop point, as the user would aim it, then the HAL's mapping of that pixel.
            const int32_t hx = vp.x + static_cast<int32_t>((static_cast<int64_t>(c.x + target.x) * vp.width + vp.width / 2) / logical.width);
            const int32_t hy = vp.y + static_cast<int32_t>((static_cast<int64_t>(c.y + target.y) * vp.height + vp.height / 2) / logical.height);
            const hal::LogicalPoint d = hal::window_to_logical(vp, hx, hy);
            CHECK(std::abs(d.x - (c.x + target.x)) <= 2 && std::abs(d.y - (c.y + target.y)) <= 2); // within one host pixel
            if (!client_contains(c, d.x, d.y)) continue; // a downscaled host cannot address every desktop pixel
            r.app.events.clear();
            r.pointer.click({d.x, d.y});
            r.context() = "host " + std::to_string(host.w) + "x" + std::to_string(host.h) + ", target (" + std::to_string(target.x) + "," +
                          std::to_string(target.y) + ")";
            CHECK(received_exactly(r.app.events, {{d.x - c.x, d.y - c.y, 1}, {d.x - c.x, d.y - c.y, 0}}));
        }
    }
    std::printf("test_viewport_scaling passed\n");
}

void test_decorations_are_not_application_input() {
    Rig r;
    const auto c = r.client();
    const auto bounds = r.wm->get_decorated_window_bounds(r.win);

    struct Press {
        std::string what;
        Point at;
        bool control;
    };
    // The title bar, every resize zone, the controls, and every pixel row/column of the frame around the client area.
    std::vector<Press> presses = {{"title bar", vynex_test::title_bar_point(*r.wm, r.win), false}};
    for (const auto edge : {ResizeEdge::Left, ResizeEdge::Right, ResizeEdge::Top, ResizeEdge::Bottom, ResizeEdge::TopLeft, ResizeEdge::TopRight,
                            ResizeEdge::BottomLeft, ResizeEdge::BottomRight}) {
        presses.push_back({"resize zone " + std::to_string(static_cast<int>(edge)), vynex_test::resize_point(*r.wm, r.win, edge), false});
    }
    for (const auto control : {DecorationHitRegion::CloseButton, DecorationHitRegion::MaximizeButton, DecorationHitRegion::MinimizeButton}) {
        presses.push_back({"control " + std::to_string(static_cast<int>(control)), vynex_test::control_point(*r.wm, r.win, control), true});
    }
    const int32_t right = c.x + static_cast<int32_t>(c.width), bottom = c.y + static_cast<int32_t>(c.height);
    // The corner resize grips reach into the client area's bottom corners: a press there resizes, so it is not input.
    for (const Point p : {Point{right - 1, bottom - 1}, Point{c.x, bottom - 1}}) {
        CHECK(r.wm->get_resize_handle_at(p.x, p.y).has_value());
        presses.push_back({"corner grip over the client at (" + std::to_string(p.x - c.x) + "," + std::to_string(p.y - c.y) + ")", p, false});
    }
    for (int32_t y = bounds.y; y < c.y; ++y) presses.push_back({"frame above the client, y=" + std::to_string(y), {c.x + 5, y}, false});
    for (int32_t y = bottom; y < bounds.y + static_cast<int32_t>(bounds.height); ++y) presses.push_back({"frame below, y=" + std::to_string(y), {c.x + 5, y}, false});
    for (int32_t x = bounds.x; x < c.x; ++x) presses.push_back({"frame left, x=" + std::to_string(x), {x, c.y + 5}, false});
    for (int32_t x = right; x < bounds.x + static_cast<int32_t>(bounds.width); ++x) presses.push_back({"frame right, x=" + std::to_string(x), {x, c.y + 5}, false});

    const Point away{10, 10}; // the wallpaper: a control released here does not activate
    CHECK(r.wm->get_window_at(away.x, away.y) != std::optional<uint32_t>{r.win});
    for (const Press& p : presses) {
        r.context() = "press on " + p.what;
        r.app.events.clear();
        r.pointer.send(p.at, 0); // hover
        r.pointer.send(p.at, 1); // press
        r.pointer.send(p.control ? away : p.at, 0);
        r.settle();
        CHECK(r.app.events.empty());
        CHECK(r.wm->get_window_geometry(r.win).has_value() && r.wm->get_window_state(r.win) == WindowState::Normal);
        const auto now = r.client();
        CHECK(now.x == c.x && now.y == c.y && now.width == c.width && now.height == c.height); // nothing moved or resized
    }
    std::printf("test_decorations_are_not_application_input passed (%zu decoration points)\n", presses.size());
}

void test_press_inside_captures_until_release() {
    Rig r;
    const auto c0 = r.client();
    r.app.events.clear();
    r.pointer.send(r.at(10, 10), 1);                 // press in the content
    r.pointer.send(r.at(-20, -30), 1);               // drag out over the title bar...
    r.pointer.send(r.at(500, 400), 1);               // ...and past the window
    r.pointer.send(r.at(500, 400), 0);               // release outside
    r.pointer.send(r.at(501, 400), 0);               // free motion outside afterwards
    r.settle();
    r.context() = "press inside, drag and release outside";
    CHECK(received_exactly(r.app.events, {{10, 10, 1}, {-20, -30, 1}, {500, 400, 1}, {500, 400, 0}}));
    CHECK(r.client().x == c0.x && r.client().y == c0.y); // the window did not move

    // A press that began elsewhere is not delivered, not even when it crosses the content, nor its release there.
    r.app.events.clear();
    const Point desktop_spot{c0.x + 600, c0.y + 50};
    CHECK(r.wm->get_window_at(desktop_spot.x, desktop_spot.y) != std::optional<uint32_t>{r.win});
    r.pointer.send(desktop_spot, 1);
    r.pointer.send(r.at(100, 100), 1);
    r.pointer.send(r.at(120, 100), 1);
    r.pointer.send(r.at(120, 100), 0);
    r.settle();
    r.context() = "press on the desktop, dragged into the content";
    CHECK(r.app.events.empty());
    std::printf("test_press_inside_captures_until_release passed\n");
}

// ---- P-4: surfaces follow the client area ---------------------------------------------------------------------------

void test_surface_follows_resizing_from_every_edge() {
    Rig r;
    r.app.state = 42;
    auto* const instance = &r.app;
    struct Step {
        const char* what;
        ResizeEdge edge;
        int32_t dx, dy;
    };
    const Step steps[] = {
        {"right edge out", ResizeEdge::Right, 120, 0},     {"bottom edge out", ResizeEdge::Bottom, 0, 90},
        {"left edge in", ResizeEdge::Left, 60, 0},         {"top edge in", ResizeEdge::Top, 0, 40},
        {"bottom-right in", ResizeEdge::BottomRight, -150, -100}, {"top-left out", ResizeEdge::TopLeft, -80, -70},
        {"top-right", ResizeEdge::TopRight, 50, 30},       {"bottom-left", ResizeEdge::BottomLeft, -40, 60},
    };
    for (const Step& s : steps) {
        r.context() = std::string("resize: ") + s.what;
        const auto before = r.client();
        const Rect old_bounds = r.wm->get_decorated_window_bounds(r.win);
        const Point from = vynex_test::resize_point(*r.wm, r.win, s.edge);
        r.app.events.clear();
        const int renders = r.app.renders;
        r.pointer.drag(from, {from.x + s.dx, from.y + s.dy});
        r.settle();
        const auto after = r.client();
        const bool left = s.edge == ResizeEdge::Left || s.edge == ResizeEdge::TopLeft || s.edge == ResizeEdge::BottomLeft;
        const bool right = s.edge == ResizeEdge::Right || s.edge == ResizeEdge::TopRight || s.edge == ResizeEdge::BottomRight;
        const bool top = s.edge == ResizeEdge::Top || s.edge == ResizeEdge::TopLeft || s.edge == ResizeEdge::TopRight;
        const bool bottom = s.edge == ResizeEdge::Bottom || s.edge == ResizeEdge::BottomLeft || s.edge == ResizeEdge::BottomRight;
        CHECK(static_cast<int64_t>(after.width) == static_cast<int64_t>(before.width) + (right ? s.dx : 0) - (left ? s.dx : 0));
        CHECK(static_cast<int64_t>(after.height) == static_cast<int64_t>(before.height) + (bottom ? s.dy : 0) - (top ? s.dy : 0));
        CHECK(r.app.events.empty());              // resizing is not application input
        CHECK(r.app.renders == renders + 1);      // rendered once, at the new size
        r.expect_content_matches_client();        // surface == client area, shown as is
        r.expect_vacated_area_is_wallpaper(old_bounds);
        CHECK(&r.app == instance && r.app.initializations == 1 && r.app.state == 42); // same instance, state kept
        // Input lands correctly in the resized content, including its new last column and row.
        const int32_t fx = static_cast<int32_t>(after.width) - 1, fy = static_cast<int32_t>(after.height) - 1;
        CHECK(received_exactly(r.click_content(fx, fy / 2), {{fx, fy / 2, 1}, {fx, fy / 2, 0}}));
        CHECK(received_exactly(r.click_content(fx / 2, fy), {{fx / 2, fy, 1}, {fx / 2, fy, 0}}));
        CHECK(received_exactly(r.click_content(0, 0), {{0, 0, 1}, {0, 0, 0}}));
    }
    std::printf("test_surface_follows_resizing_from_every_edge passed (%zu resizes)\n", std::size(steps));
}

void test_surfaces_are_replaced_only_when_the_size_changes() {
    Rig r;
    std::vector<std::weak_ptr<ISurface>> seen{r.content_surface()};
    const auto c0 = r.client();

    r.context() = "idle frames, moving, and re-applying the same geometry";
    r.compositor->created.clear();
    {
        const auto first = r.content_surface(); // held only within this block, so the release check below is not affected
        for (int i = 0; i < 10; ++i) r.frame();
        const Point grab = vynex_test::title_bar_point(*r.wm, r.win);
        r.pointer.drag(grab, {grab.x + 33, grab.y + 21});
        r.settle();
        r.wm->set_window_geometry(r.win, r.client()); // same size again
        r.settle();
        CHECK(r.compositor->created.empty()); // no surface of any size: neither content nor chrome
        CHECK(r.content_surface() == first);
    }

    r.context() = "repeated resizing";
    for (int i = 0; i < 12; ++i) {
        auto g = r.client();
        g.width = c0.width + static_cast<uint32_t>((i % 3) * 37);
        g.height = c0.height + static_cast<uint32_t>((i % 4) * 19);
        const bool changes = g.width != r.client().width || g.height != r.client().height;
        const int before = r.compositor->created_of(g.width, g.height);
        r.wm->set_window_geometry(r.win, g);
        r.settle();
        CHECK(r.compositor->created_of(g.width, g.height) == before + (changes ? 1 : 0)); // one content surface per change
        seen.push_back(r.content_surface());
        r.expect_content_matches_client();
    }
    // Only the surface in use is alive: replaced surfaces are released, not retained by the shell or window manager.
    const auto current = r.content_surface();
    const auto alive = std::count_if(seen.begin(), seen.end(), [](const std::weak_ptr<ISurface>& w) { return !w.expired(); });
    r.context() = "replaced surfaces released";
    CHECK(alive == 1);
    CHECK(seen.back().lock() == current);
    std::printf("test_surfaces_are_replaced_only_when_the_size_changes passed\n");
}

void test_maximize_restore_and_minimize() {
    Rig r;
    r.app.state = 7;
    const auto normal = r.client();
    const auto work = r.wm->get_work_area();

    r.pointer.click(vynex_test::control_point(*r.wm, r.win, DecorationHitRegion::MaximizeButton));
    r.settle();
    r.context() = "maximized";
    CHECK(r.wm->get_window_state(r.win) == WindowState::Maximized);
    const auto max = r.client();
    const Rect max_bounds = r.wm->get_decorated_window_bounds(r.win); // the maximized frame fills the work area
    CHECK(max_bounds.x == work.x && max_bounds.y == work.y && max_bounds.width == work.width && max_bounds.height == work.height);
    const auto expected = client_within(work, r.wm->get_window_margins(r.win));
    CHECK(max.x == expected.x && max.y == expected.y && max.width == expected.width && max.height == expected.height);
    r.expect_content_matches_client();
    const int32_t mx = static_cast<int32_t>(max.width) - 1, my = static_cast<int32_t>(max.height) - 1;
    CHECK(received_exactly(r.click_content(mx, my), {{mx, my, 1}, {mx, my, 0}}));

    r.pointer.click(vynex_test::control_point(*r.wm, r.win, DecorationHitRegion::MaximizeButton));
    r.settle();
    r.context() = "restored";
    CHECK(r.wm->get_window_state(r.win) == WindowState::Normal);
    CHECK(r.client().x == normal.x && r.client().y == normal.y && r.client().width == normal.width && r.client().height == normal.height);
    r.expect_content_matches_client();
    r.expect_vacated_area_is_wallpaper(max_bounds);
    CHECK(received_exactly(r.click_content(399, 150), {{399, 150, 1}, {399, 150, 0}}));
    CHECK(received_exactly(r.click_content(200, 299), {{200, 299, 1}, {200, 299, 0}}));

    // Minimize and restore: same size, so the same surface; still painted correctly afterwards.
    const auto surface = r.content_surface();
    const int created = r.compositor->created_of(normal.width, normal.height);
    r.pointer.click(vynex_test::control_point(*r.wm, r.win, DecorationHitRegion::MinimizeButton));
    r.settle();
    CHECK(r.wm->get_window_state(r.win) == WindowState::Minimized);
    r.wm->set_window_state(r.win, WindowState::Normal);
    r.shell->focus_window(r.win);
    r.settle();
    r.context() = "minimized and restored";
    CHECK(r.content_surface() == surface);
    CHECK(r.compositor->created_of(normal.width, normal.height) == created); // no new surface for the same size
    r.expect_content_matches_client();
    CHECK(r.app.initializations == 1 && r.app.state == 7);
    std::printf("test_maximize_restore_and_minimize passed\n");
}

void test_minimum_size_keeps_the_opposite_edge() {
    Rig r;
    const auto c0 = r.client();
    const int64_t right = static_cast<int64_t>(c0.x) + c0.width, bottom = static_cast<int64_t>(c0.y) + c0.height;

    Point from = vynex_test::resize_point(*r.wm, r.win, ResizeEdge::Left);
    r.pointer.drag(from, {from.x + 1000, from.y}); // far past the minimum width
    r.settle();
    r.context() = "left edge dragged past the minimum width";
    CHECK(r.client().width == 100);                                                   // the window manager's minimum
    CHECK(static_cast<int64_t>(r.client().x) + r.client().width == right);            // right edge unchanged
    r.expect_content_matches_client();

    from = vynex_test::resize_point(*r.wm, r.win, ResizeEdge::Top);
    r.pointer.drag(from, {from.x, from.y + 1000}); // far past the minimum height
    r.settle();
    r.context() = "top edge dragged past the minimum height";
    CHECK(r.client().height == 50);
    CHECK(static_cast<int64_t>(r.client().y) + r.client().height == bottom);          // bottom edge unchanged
    r.expect_content_matches_client();
    CHECK(received_exactly(r.click_content(99, 25), {{99, 25, 1}, {99, 25, 0}}));
    CHECK(received_exactly(r.click_content(50, 49), {{50, 49, 1}, {50, 49, 0}}));
    std::printf("test_minimum_size_keeps_the_opposite_edge passed\n");
}

void test_zero_sized_client_area() {
    Rig r;
    const int renders = r.app.renders;
    // A work area too small for the maximized title bar leaves no client area at all.
    r.wm->set_work_area({0, 0, 300, 10});
    r.compositor->created.clear();
    r.pointer.click(vynex_test::control_point(*r.wm, r.win, DecorationHitRegion::MaximizeButton));
    for (int i = 0; i < 5; ++i) r.frame();
    r.context() = "maximized into no space";
    CHECK(r.wm->get_window_state(r.win) == WindowState::Maximized);
    CHECK(r.client().height == 0);
    CHECK(r.app.renders == renders);                 // nothing to render into...
    CHECK(r.compositor->created_of(r.client().width, 0) == 0); // ...and no empty surface made for it

    r.wm->set_window_state(r.win, WindowState::Normal);
    r.shell->focus_window(r.win);
    r.settle();
    r.context() = "restored from no space";
    CHECK(r.app.renders == renders + 1); // painted in full again
    r.expect_content_matches_client();
    std::printf("test_zero_sized_client_area passed\n");
}

} // namespace

int main() {
    std::printf("Running application geometry tests...\n");
    test_clicks_arrive_in_content_coordinates();
    test_window_positions_and_frame_sizes();
    test_moving_the_window();
    test_viewport_scaling();
    test_decorations_are_not_application_input();
    test_press_inside_captures_until_release();
    test_surface_follows_resizing_from_every_edge();
    test_surfaces_are_replaced_only_when_the_size_changes();
    test_maximize_restore_and_minimize();
    test_minimum_size_keeps_the_opposite_edge();
    test_zero_sized_client_area();
    std::printf("All application geometry tests passed.\n");
    return 0;
}
