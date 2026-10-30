// Window chrome interaction: hover and pressed feedback, pointer capture with release-based activation, cancellation,
// surface reuse with partial repaints, and the compositor's handling of decoration layers.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <crtdbg.h>
#include <cstdlib>

#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/hal/input_driver.hpp"

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

/// Forwards to a real compositor while recording every surface allocation and every invalidated rectangle.
class RecordingCompositor final : public ICompositor {
public:
    explicit RecordingCompositor(std::shared_ptr<BasicCompositor> inner) : m_inner(std::move(inner)) {}
    std::shared_ptr<ISurface> create_surface(uint32_t width, uint32_t height) override {
        ++surfaces_created;
        return m_inner->create_surface(width, height);
    }
    void invalidate_region(const Rect& rect) override {
        invalidated.push_back(rect);
        m_inner->invalidate_region(rect);
    }
    std::expected<void, DisplayError> render_frame(const SceneGraph& scene) override { return m_inner->render_frame(scene); }
    hal::DisplaySize logical_size() const override { return m_inner->logical_size(); }

    int surfaces_created{0};
    std::vector<Rect> invalidated;

private:
    std::shared_ptr<BasicCompositor> m_inner;
};

bool contains(const Rect& outer, const Rect& inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + static_cast<int64_t>(inner.width) <= outer.x + static_cast<int64_t>(outer.width) &&
           inner.y + static_cast<int64_t>(inner.height) <= outer.y + static_cast<int64_t>(outer.height);
}

bool inside(const Rect& r, int32_t x, int32_t y) {
    return x >= r.x && y >= r.y && x < r.x + static_cast<int64_t>(r.width) && y < r.y + static_cast<int64_t>(r.height);
}

bool same_rect(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }

std::vector<uint8_t> pixels_of(ISurface& s) {
    auto m = s.map_pixels();
    std::vector<uint8_t> v(m.begin(), m.end());
    s.unmap_pixels();
    return v;
}

/// A desktop with one ordinary window ("Alpha") and, optionally, a second one ("Beta") to its right.
struct Rig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>();
    std::shared_ptr<RecordingCompositor> compositor = std::make_shared<RecordingCompositor>(std::make_shared<BasicCompositor>(display, logger));
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<BasicWindowDecorator> decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    std::shared_ptr<BasicTaskbar> taskbar = std::make_shared<BasicTaskbar>(toolkit);
    std::unique_ptr<apps::DesktopShell> shell;
    uint32_t alpha{0};
    uint32_t beta{0};

    explicit Rig(bool with_beta = false) {
        shell = std::make_unique<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        shell->launch();
        alpha = add_window("Alpha", {300, 300, 500, 300});
        if (with_beta) beta = add_window("Beta", {1000, 300, 400, 300});
        shell->update_frame(); // gives every window its chrome
        settle();
    }

    uint32_t add_window(const char* title, WindowGeometry geom) {
        const uint32_t id = wm->create_window(title, geom);
        wm->set_window_surface(id, compositor->create_surface(geom.width, geom.height));
        return id;
    }

    // Runs a frame and forgets what it allocated and invalidated, so a test sees only what its own actions caused.
    void settle() {
        shell->update_frame();
        (void)compositor->render_frame(wm->build_scene());
        compositor->surfaces_created = 0;
        compositor->invalidated.clear();
    }

    void mouse(int32_t x, int32_t y, uint8_t buttons) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputMouse;
        ev->payload = hal::MouseEvent(x, y, buttons);
        bus->dispatch_immediate(ev);
    }
    void click(int32_t x, int32_t y) {
        mouse(x, y, 1);
        mouse(x, y, 0);
    }
    void pointer_signal(hal::PointerSignal s) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputPointer;
        ev->payload = hal::PointerSignalEvent{s};
        bus->dispatch_immediate(ev);
    }

    Rect bounds(uint32_t id) const { return wm->get_decorated_window_bounds(id); }
    DecorationLayout layout(uint32_t id) const {
        const auto g = wm->get_window_geometry(id);
        return calculate_decoration_layout(make_frame_metrics(wm->get_window_frame_style(id), bounds(id), {g->x, g->y, g->width, g->height}));
    }
    static Rect control_rect(const DecorationLayout& l, DecorationHitRegion c) {
        switch (c) {
        case DecorationHitRegion::CloseButton: return l.close_button;
        case DecorationHitRegion::MinimizeButton: return l.minimize_button;
        case DecorationHitRegion::MaximizeButton: return l.maximize_button;
        default: return {0, 0, 0, 0};
        }
    }
    /// A control's rectangle in desktop coordinates.
    Rect screen_rect(uint32_t id, DecorationHitRegion c) const {
        const Rect b = bounds(id);
        const Rect r = control_rect(layout(id), c);
        return {b.x + r.x, b.y + r.y, r.width, r.height};
    }
    std::pair<int32_t, int32_t> centre(uint32_t id, DecorationHitRegion c) const {
        const Rect r = screen_rect(id, c);
        return {r.x + static_cast<int32_t>(r.width / 2), r.y + static_cast<int32_t>(r.height / 2)};
    }
    /// A point on the title bar well away from the controls.
    std::pair<int32_t, int32_t> title_point(uint32_t id) const {
        const Rect b = bounds(id);
        const auto l = layout(id);
        return {b.x + l.title_bar.x + 120, b.y + l.title_bar.y + static_cast<int32_t>(l.title_bar.height / 2)};
    }

    /// The chrome surface the window manager composites for a window.
    std::shared_ptr<ISurface> chrome(uint32_t id) const {
        const Rect b = bounds(id);
        for (const auto& layer : wm->build_scene()) {
            if (layer.window_id == id && same_rect(layer.bounds, b)) return layer.surface;
        }
        return nullptr;
    }
    /// What the chrome must look like for the given interaction state, drawn from scratch by the decorator.
    std::vector<uint8_t> expected(uint32_t id, DecorationHitRegion hovered, DecorationHitRegion pressed, bool focused = false) const {
        const Rect b = bounds(id);
        auto s = std::make_shared<SoftwareSurface>(b.width, b.height);
        DecorationVisualState v;
        v.focused = focused;
        v.state = wm->get_window_state(id);
        v.hovered = hovered;
        v.pressed = pressed;
        decorator->draw_decorations(s, layout(id), wm->get_window_title(id), v, WindowTheme{});
        return pixels_of(*s);
    }
    bool chrome_is(uint32_t id, DecorationHitRegion hovered, DecorationHitRegion pressed, bool focused = false) const {
        auto s = chrome(id);
        return s && pixels_of(*s) == expected(id, hovered, pressed, focused);
    }
    /// True when every invalidation since settle() is a cursor patch or lies inside one of `allowed`.
    bool invalidated_only(std::initializer_list<Rect> allowed) const {
        for (const Rect& r : compositor->invalidated) {
            if (r.width == 16 && r.height == 16) continue; // the shell's cursor patch, sent for every pointer event
            if (!std::any_of(allowed.begin(), allowed.end(), [&](const Rect& a) { return contains(a, r); })) return false;
        }
        return true;
    }
    bool any_non_cursor_invalidation() const {
        return std::any_of(compositor->invalidated.begin(), compositor->invalidated.end(), [](const Rect& r) { return !(r.width == 16 && r.height == 16); });
    }
};

using H = DecorationHitRegion;

// ---------------------------------------------------------------------------------------------------------
// Decorator: partial repaints are exact
// ---------------------------------------------------------------------------------------------------------
void test_partial_repaint_matches_full_redraw() {
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    BasicWindowDecorator decorator(toolkit);
    const H controls[] = {H::MinimizeButton, H::MaximizeButton, H::CloseButton};

    for (const WindowState state : {WindowState::Normal, WindowState::Maximized}) {
        const FrameStyle style = decorator.frame_style(state);
        const uint32_t cw = 500, ch = 300;
        const uint32_t w = cw + static_cast<uint32_t>(style.margins.left + style.margins.right);
        const uint32_t h = ch + static_cast<uint32_t>(style.margins.top + style.margins.bottom);
        const auto layout = calculate_decoration_layout(make_frame_metrics(style, {0, 0, w, h}, {style.margins.left, style.margins.top, cw, ch}));

        for (const bool focused : {false, true}) {
            for (const H from : controls) {
                for (const H to : controls) {
                    for (const bool pressed : {false, true}) {
                        DecorationVisualState a;
                        a.focused = focused;
                        a.state = state;
                        a.hovered = from;
                        DecorationVisualState b = a;
                        b.hovered = to;
                        b.pressed = pressed ? to : H::None;

                        auto live = std::make_shared<SoftwareSurface>(w, h);
                        decorator.draw_decorations(live, layout, "File Explorer", a, WindowTheme{});
                        const auto before = pixels_of(*live);
                        const Rect r1 = Rig::control_rect(layout, from);
                        const Rect r2 = Rig::control_rect(layout, to);
                        assert(decorator.redraw_decoration_region(live, layout, "File Explorer", b, WindowTheme{}, r1));
                        assert(decorator.redraw_decoration_region(live, layout, "File Explorer", b, WindowTheme{}, r2));

                        auto fresh = std::make_shared<SoftwareSurface>(w, h);
                        decorator.draw_decorations(fresh, layout, "File Explorer", b, WindowTheme{});
                        const auto after = pixels_of(*live);
                        assert(after == pixels_of(*fresh)); // identical to drawing everything again

                        // ...and nothing outside the two control rectangles was touched.
                        for (uint32_t y = 0; y < h; ++y) {
                            for (uint32_t x = 0; x < w; ++x) {
                                if (inside(r1, static_cast<int32_t>(x), static_cast<int32_t>(y)) || inside(r2, static_cast<int32_t>(x), static_cast<int32_t>(y))) continue;
                                const size_t i = (static_cast<size_t>(y) * w + x) * 4;
                                assert(std::equal(before.begin() + static_cast<std::ptrdiff_t>(i), before.begin() + static_cast<std::ptrdiff_t>(i + 4),
                                                  after.begin() + static_cast<std::ptrdiff_t>(i)));
                            }
                        }
                    }
                }
            }
        }

        // A region reaching the title text cannot be repainted alone; the caller is told to redraw everything.
        auto live = std::make_shared<SoftwareSurface>(w, h);
        decorator.draw_decorations(live, layout, "File Explorer", DecorationVisualState{}, WindowTheme{});
        const auto untouched = pixels_of(*live);
        assert(!decorator.redraw_decoration_region(live, layout, "File Explorer", DecorationVisualState{}, WindowTheme{},
                                                   {layout.title_bar.x + 20, layout.title_bar.y, 40, layout.title_bar.height}));
        assert(pixels_of(*live) == untouched); // a refused region leaves the surface as it was
        // A region entirely off the surface is trivially done.
        assert(decorator.redraw_decoration_region(live, layout, "File Explorer", DecorationVisualState{}, WindowTheme{}, {-50, -50, 10, 10}));
        assert(pixels_of(*live) == untouched);
    }
    std::cout << "test_partial_repaint_matches_full_redraw passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Hover
// ---------------------------------------------------------------------------------------------------------
void test_hover_transitions() {
    Rig r;
    const uint32_t a = r.alpha;
    assert(r.chrome_is(a, H::None, H::None));
    const auto chrome = r.chrome(a);

    const auto [mx, my] = r.centre(a, H::MinimizeButton);
    r.mouse(mx, my, 0);
    assert(r.chrome_is(a, H::MinimizeButton, H::None));
    assert(r.chrome(a) == chrome);                                  // same surface, repainted in place
    assert(r.compositor->surfaces_created == 0);
    assert(r.invalidated_only({r.screen_rect(a, H::MinimizeButton)})); // only the control itself is redrawn on screen
    assert(r.any_non_cursor_invalidation());

    // Moving within the same control changes nothing at all.
    r.settle();
    r.mouse(mx + 2, my + 1, 0);
    assert(!r.any_non_cursor_invalidation());

    // Minimize -> maximize: the old control returns to normal, the new one lights up; only those two are touched.
    r.settle();
    const auto [xx, xy] = r.centre(a, H::MaximizeButton);
    r.mouse(xx, xy, 0);
    assert(r.chrome_is(a, H::MaximizeButton, H::None));
    assert(r.invalidated_only({r.screen_rect(a, H::MinimizeButton), r.screen_rect(a, H::MaximizeButton)}));

    // Close has its own (danger) hover look.
    const auto [cx, cy] = r.centre(a, H::CloseButton);
    r.mouse(cx, cy, 0);
    assert(r.chrome_is(a, H::CloseButton, H::None));

    // Leaving the controls restores the original pixels exactly.
    r.settle();
    const auto [tx, ty] = r.title_point(a);
    r.mouse(tx, ty, 0);
    assert(r.chrome_is(a, H::None, H::None));
    assert(r.invalidated_only({r.screen_rect(a, H::CloseButton)}));

    // So does the pointer leaving the host window.
    r.mouse(mx, my, 0);
    assert(r.chrome_is(a, H::MinimizeButton, H::None));
    r.pointer_signal(hal::PointerSignal::Left);
    assert(r.chrome_is(a, H::None, H::None));

    assert(r.compositor->surfaces_created == 0);
    std::cout << "test_hover_transitions passed\n";
}

void test_hover_moves_between_windows_and_respects_stacking() {
    Rig r(/*with_beta=*/true);
    const auto [amx, amy] = r.centre(r.alpha, H::MinimizeButton);
    const auto [bcx, bcy] = r.centre(r.beta, H::CloseButton);
    r.mouse(amx, amy, 0);
    assert(r.chrome_is(r.alpha, H::MinimizeButton, H::None));
    r.mouse(bcx, bcy, 0);
    assert(r.chrome_is(r.alpha, H::None, H::None)); // the previous window lets go
    assert(r.chrome_is(r.beta, H::CloseButton, H::None));

    // A window stacked on top hides the controls beneath it from the pointer.
    r.mouse(10, 10, 0);
    r.wm->move_window(r.beta, 500, 300); // Beta now covers the right end of Alpha's title bar, controls included
    r.shell->update_frame();
    assert(r.wm->get_window_at(amx, amy) == r.beta);
    r.mouse(amx, amy, 0);
    assert(r.chrome_is(r.alpha, H::None, H::None));
    std::cout << "test_hover_moves_between_windows_and_respects_stacking passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Pressed state, capture and release-based activation
// ---------------------------------------------------------------------------------------------------------
void test_press_shows_pressed_and_release_activates() {
    Rig r;
    const uint32_t a = r.alpha;
    const auto [cx, cy] = r.centre(a, H::CloseButton);
    r.mouse(cx, cy, 0);
    r.mouse(cx, cy, 1);
    assert(r.wm->get_window_geometry(a).has_value()); // nothing happens on press...
    assert(r.chrome_is(a, H::CloseButton, H::CloseButton));
    assert(r.invalidated_only({r.screen_rect(a, H::CloseButton)}));
    r.mouse(cx, cy, 0);
    assert(!r.wm->get_window_geometry(a).has_value()); // ...the release over the same control closes the window

    // Minimize and maximize behave the same way.
    Rig m;
    const auto [mx, my] = m.centre(m.alpha, H::MinimizeButton);
    m.mouse(mx, my, 1);
    assert(m.wm->get_window_state(m.alpha) == WindowState::Normal);
    assert(m.chrome_is(m.alpha, H::MinimizeButton, H::MinimizeButton));
    m.mouse(mx, my, 0);
    assert(m.wm->get_window_state(m.alpha) == WindowState::Minimized);

    Rig x;
    const auto [xx, xy] = x.centre(x.alpha, H::MaximizeButton);
    x.mouse(xx, xy, 1);
    assert(x.wm->get_window_state(x.alpha) == WindowState::Normal);
    x.mouse(xx, xy, 0);
    assert(x.wm->get_window_state(x.alpha) == WindowState::Maximized);
    assert(x.shell->focused_window() == x.alpha); // maximizing still focuses the window, as before
    // Restore from the same place (the button is now the restore button of the maximized frame).
    const auto [rx, ry] = x.centre(x.alpha, H::MaximizeButton);
    x.click(rx, ry);
    assert(x.wm->get_window_state(x.alpha) == WindowState::Normal);
    std::cout << "test_press_shows_pressed_and_release_activates passed\n";
}

void test_leaving_the_control_cancels_and_reentering_rearms() {
    Rig r;
    const uint32_t a = r.alpha;
    const auto [mx, my] = r.centre(a, H::MinimizeButton);
    const auto [tx, ty] = r.title_point(a);
    r.mouse(mx, my, 1);
    assert(r.chrome_is(a, H::MinimizeButton, H::MinimizeButton));

    r.mouse(tx, ty, 1); // dragged off the control: shown normal, still captured
    assert(r.chrome_is(a, H::None, H::None));
    assert(r.bounds(a).x == 293); // a press that began on a control never turns into a window drag

    r.mouse(mx, my, 1); // back over it: pressed again
    assert(r.chrome_is(a, H::MinimizeButton, H::MinimizeButton));

    r.mouse(tx, ty, 1);
    r.mouse(tx, ty, 0); // released elsewhere: nothing happens
    assert(r.wm->get_window_state(a) == WindowState::Normal);
    assert(r.chrome_is(a, H::None, H::None));

    // Releasing over a DIFFERENT control of the same window does not activate either of them.
    const auto [cx, cy] = r.centre(a, H::CloseButton);
    r.mouse(mx, my, 1);
    r.mouse(cx, cy, 1);
    assert(r.chrome_is(a, H::None, H::None)); // capture: the close button gets no hover while minimize holds the pointer
    r.mouse(cx, cy, 0);
    assert(r.wm->get_window_geometry(a).has_value());
    assert(r.wm->get_window_state(a) == WindowState::Normal);
    assert(r.chrome_is(a, H::CloseButton, H::None)); // after the release, hover resumes under the pointer
    std::cout << "test_leaving_the_control_cancels_and_reentering_rearms passed\n";
}

void test_capture_ignores_other_windows() {
    Rig r(/*with_beta=*/true);
    const auto [amx, amy] = r.centre(r.alpha, H::MinimizeButton);
    const auto [bcx, bcy] = r.centre(r.beta, H::CloseButton);
    r.mouse(amx, amy, 1);
    r.mouse(bcx, bcy, 1);
    assert(r.chrome_is(r.beta, H::None, H::None)); // no hover feedback on another window during capture
    r.mouse(bcx, bcy, 0);
    assert(r.wm->get_window_geometry(r.beta).has_value()); // and no activation there
    assert(r.wm->get_window_state(r.alpha) == WindowState::Normal);
    std::cout << "test_capture_ignores_other_windows passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Cancellation and interruption
// ---------------------------------------------------------------------------------------------------------
void test_cancel_signal_abandons_press_without_activating() {
    Rig r;
    const uint32_t a = r.alpha;
    const auto [cx, cy] = r.centre(a, H::CloseButton);
    r.mouse(cx, cy, 1);
    r.pointer_signal(hal::PointerSignal::Cancel); // e.g. the host window lost focus mid-press
    assert(r.chrome_is(a, H::None, H::None));

    // The button may still be down when focus returns, and the release can arrive over the control: neither acts.
    r.mouse(cx, cy, 1);
    assert(r.chrome_is(a, H::None, H::None));
    r.mouse(cx, cy, 0);
    assert(r.wm->get_window_geometry(a).has_value());

    // Afterwards the pointer works normally again.
    assert(r.chrome_is(a, H::CloseButton, H::None));
    r.click(cx, cy);
    assert(!r.wm->get_window_geometry(a).has_value());
    std::cout << "test_cancel_signal_abandons_press_without_activating passed\n";
}

void test_cancel_signal_ends_drag_and_resize() {
    Rig r;
    const uint32_t a = r.alpha;
    const auto [tx, ty] = r.title_point(a);
    r.mouse(tx, ty, 1);
    r.mouse(tx + 40, ty + 30, 1);
    assert(r.wm->get_window_geometry(a)->x == 340); // dragging works...
    r.pointer_signal(hal::PointerSignal::Cancel);
    r.mouse(tx + 200, ty + 100, 1);
    assert(r.wm->get_window_geometry(a)->x == 340); // ...and stops where it was when cancelled
    r.mouse(tx + 200, ty + 100, 0);

    // Resize from the right edge, then cancel.
    const Rect b = r.bounds(a);
    const int32_t ex = b.x + static_cast<int32_t>(b.width) - 3, ey = b.y + static_cast<int32_t>(b.height / 2);
    r.mouse(ex, ey, 1);
    r.mouse(ex + 50, ey, 1);
    assert(r.wm->get_window_geometry(a)->width == 550);
    r.pointer_signal(hal::PointerSignal::Cancel);
    r.mouse(ex + 150, ey, 1);
    assert(r.wm->get_window_geometry(a)->width == 550);
    r.mouse(ex + 150, ey, 0);
    std::cout << "test_cancel_signal_ends_drag_and_resize passed\n";
}

void test_window_change_during_press_interrupts_it() {
    // Closed by its application while a control is held.
    {
        Rig r(/*with_beta=*/true);
        const auto [mx, my] = r.centre(r.alpha, H::MinimizeButton);
        r.mouse(mx, my, 1);
        r.wm->destroy_window(r.alpha);
        r.mouse(mx, my, 1);
        r.mouse(mx, my, 0); // must not touch anything (and must not crash)
        assert(r.wm->get_window_geometry(r.beta).has_value());
        const auto [bx, by] = r.centre(r.beta, H::MinimizeButton);
        r.mouse(bx, by, 0);
        assert(r.chrome_is(r.beta, H::MinimizeButton, H::None)); // the pointer is free again
    }
    // Changed state from elsewhere while the close button is held: the release is ignored.
    {
        Rig r;
        const auto [cx, cy] = r.centre(r.alpha, H::CloseButton);
        r.mouse(cx, cy, 1);
        r.wm->set_window_state(r.alpha, WindowState::Maximized);
        r.mouse(cx, cy, 0);
        assert(r.wm->get_window_geometry(r.alpha).has_value());
    }
    std::cout << "test_window_change_during_press_interrupts_it passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Preserved behaviour and surface reuse
// ---------------------------------------------------------------------------------------------------------
void test_drag_resize_focus_still_work() {
    Rig r(/*with_beta=*/true);
    const uint32_t a = r.alpha;
    const auto [tx, ty] = r.title_point(a);
    r.mouse(tx, ty, 1);
    r.mouse(tx + 25, ty + 15, 1);
    r.mouse(tx + 25, ty + 15, 0);
    assert(r.wm->get_window_geometry(a)->x == 325 && r.wm->get_window_geometry(a)->y == 315);
    assert(r.shell->focused_window() == a);

    const auto [btx, bty] = r.title_point(r.beta);
    r.click(btx, bty);
    assert(r.shell->focused_window() == r.beta);
    assert(r.chrome_is(a, H::None, H::None, /*focused=*/false));
    assert(r.chrome_is(r.beta, H::None, H::None, /*focused=*/true));
    std::cout << "test_drag_resize_focus_still_work passed\n";
}

void test_surface_reuse_and_no_redundant_redraws() {
    Rig r(/*with_beta=*/true);
    const uint32_t a = r.alpha;
    const auto chrome_a = r.chrome(a);
    const auto chrome_b = r.chrome(r.beta);

    // A whole session of ordinary interaction: hovering, pressing, cancelling, focusing, re-clicking a focused window.
    const auto [tx, ty] = r.title_point(a);
    r.click(tx, ty);                         // focus Alpha: its chrome is redrawn (focus look), in the same surface
    assert(r.chrome(a) == chrome_a);
    assert(r.chrome_is(a, H::None, H::None, /*focused=*/true));
    for (const H c : {H::MinimizeButton, H::MaximizeButton, H::CloseButton}) {
        const auto [x, y] = r.centre(a, c);
        r.mouse(x, y, 0);
        r.mouse(x, y, 1);
        r.mouse(tx, ty, 1);
        r.mouse(tx, ty, 0);
    }
    const auto [btx, bty] = r.title_point(r.beta);
    r.click(btx, bty);
    r.click(tx, ty);
    r.shell->update_frame();
    assert(r.compositor->surfaces_created == 0); // not one new surface
    assert(r.chrome(a) == chrome_a && r.chrome(r.beta) == chrome_b);

    // Clicking the title bar of the window that already has focus repaints nothing.
    r.settle();
    r.click(tx, ty);
    r.shell->update_frame();
    assert(!r.any_non_cursor_invalidation());

    // Only a new size needs a new surface (maximize changes the frame size).
    const auto [xx, xy] = r.centre(a, H::MaximizeButton);
    r.click(xx, xy);
    assert(r.wm->get_window_state(a) == WindowState::Maximized);
    assert(r.compositor->surfaces_created == 1);
    std::cout << "test_surface_reuse_and_no_redundant_redraws passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Compositor
// ---------------------------------------------------------------------------------------------------------
void test_decoration_layer_is_clipped_to_the_visible_frame() {
    Rig r;
    const uint32_t a = r.alpha;
    const Rect b = r.bounds(a);
    const auto frame = r.layout(a).visible_frame;
    bool found = false;
    for (const auto& layer : r.wm->build_scene()) {
        if (layer.window_id == a && same_rect(layer.bounds, b)) {
            found = true;
            assert(same_rect(layer.clip, Rect{b.x + frame.x, b.y + frame.y, frame.width, frame.height}));
            assert(frame.x == 6 && frame.width == b.width - 12); // the 6 px band is outside the clip
        }
    }
    assert(found);
    // Maximized windows have no band: the clip is the whole frame.
    r.wm->set_window_state(a, WindowState::Maximized);
    r.shell->update_frame();
    const Rect mb = r.bounds(a);
    for (const auto& layer : r.wm->build_scene()) {
        if (layer.window_id == a && same_rect(layer.bounds, mb)) assert(same_rect(layer.clip, mb));
    }
    std::cout << "test_decoration_layer_is_clipped_to_the_visible_frame passed\n";
}

void test_span_blending_matches_per_pixel_blending() {
    auto logger = std::make_shared<TestLogger>();
    auto display = std::make_shared<CapturingDisplay>();
    BasicCompositor compositor(display, logger);

    // Two overlapping layers with long opaque runs, long transparent runs and scattered partial alpha.
    const uint32_t w = 97, h = 41;
    uint32_t seed = 12345;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return seed >> 24; };
    auto make = [&](bool opaque) {
        auto s = std::make_shared<SoftwareSurface>(w, h);
        auto m = s->map_pixels();
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const size_t i = (static_cast<size_t>(y) * w + x) * 4;
                m[i] = static_cast<uint8_t>(rnd());
                m[i + 1] = static_cast<uint8_t>(rnd());
                m[i + 2] = static_cast<uint8_t>(rnd());
                const uint32_t band = (x / 13 + y / 7) % 4;
                m[i + 3] = opaque ? 255 : static_cast<uint8_t>(band == 0 ? 0 : band == 1 ? 255 : band == 2 ? rnd() : (x % 2 ? 0 : 255));
            }
        }
        s->unmap_pixels();
        return s;
    };
    auto bottom = make(true);
    auto top = make(false);
    SceneGraph scene(2);
    scene[0].bounds = {10, 10, w, h};
    scene[0].clip = {0, 0, 0, 0};
    scene[0].surface = bottom;
    scene[1].bounds = {40, 25, w, h};
    scene[1].clip = {45, 27, w - 9, h - 4}; // clipped, so runs start and end mid-row
    scene[1].surface = top;
    assert(compositor.render_frame(scene).has_value());
    const auto& fb = display->last;

    // Reference: the original per-pixel rule, applied naively.
    std::vector<uint8_t> ref(static_cast<size_t>(fb.width) * fb.height * 4, 0);
    auto blend = [&](const std::shared_ptr<SoftwareSurface>& s, const Rect& at, const Rect& clip) {
        auto m = s->map_pixels();
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const int32_t dx = at.x + static_cast<int32_t>(x), dy = at.y + static_cast<int32_t>(y);
                if (!inside(clip, dx, dy)) continue;
                const size_t si = (static_cast<size_t>(y) * w + x) * 4;
                const size_t di = (static_cast<size_t>(dy) * fb.width + static_cast<size_t>(dx)) * 4;
                const uint8_t sa = m[si + 3];
                if (sa == 255) {
                    ref[di] = m[si]; ref[di + 1] = m[si + 1]; ref[di + 2] = m[si + 2]; ref[di + 3] = 255;
                } else if (sa > 0) {
                    const float al = sa / 255.0f, inv = 1.0f - al;
                    ref[di] = static_cast<uint8_t>((m[si] * al) + (ref[di] * inv));
                    ref[di + 1] = static_cast<uint8_t>((m[si + 1] * al) + (ref[di + 1] * inv));
                    ref[di + 2] = static_cast<uint8_t>((m[si + 2] * al) + (ref[di + 2] * inv));
                    ref[di + 3] = std::max(ref[di + 3], sa);
                }
            }
        }
        s->unmap_pixels();
    };
    blend(bottom, scene[0].bounds, {0, 0, fb.width, fb.height});
    blend(top, scene[1].bounds, scene[1].clip);
    assert(fb.pixels == ref);
    std::cout << "test_span_blending_matches_per_pixel_blending passed\n";
}

} // namespace

int main() {
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

    test_partial_repaint_matches_full_redraw();
    test_hover_transitions();
    test_hover_moves_between_windows_and_respects_stacking();
    test_press_shows_pressed_and_release_activates();
    test_leaving_the_control_cancels_and_reentering_rearms();
    test_capture_ignores_other_windows();
    test_cancel_signal_abandons_press_without_activating();
    test_cancel_signal_ends_drag_and_resize();
    test_window_change_during_press_interrupts_it();
    test_drag_resize_focus_still_work();
    test_surface_reuse_and_no_redundant_redraws();
    test_decoration_layer_is_clipped_to_the_visible_frame();
    test_span_blending_matches_per_pixel_blending();
    std::cout << "All chrome interaction tests passed successfully.\n";
    return 0;
}
