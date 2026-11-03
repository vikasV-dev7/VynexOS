// End-to-end presentation of application content: File Explorer is launched through the application registry and
// hosted by the real desktop shell, window manager and compositor; mouse events select rows; the test then checks the
// application's state, its surface, and the COMPOSED framebuffer that would be presented.
//
// Invariant checked after every interaction: over the window's client area, the composed frame equals the application's
// surface byte for byte. A change that is rendered into the surface but never recomposited (stale content) breaks it.
// Each change must also stay local: pixels outside the rows it affects do not change, and the compositor is asked to
// redraw only those rows (not the whole window or the desktop). The last test combines this with moving, resizing,
// maximizing and restoring the window (pointer coordinates and surface size following the window's geometry).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../apps/basic_file_explorer.hpp"
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
#include "vynexos/core/memory_file_system.hpp"
#include "vynexos/hal/input_driver.hpp"

#include "test_check.hpp"
#include "window_interaction.hpp"

using namespace vynexos;
using namespace vynexos::desktop;

namespace {

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

/// Keeps the last presented frame and counts presentations.
class CapturingDisplay final : public IDisplayAbstraction {
public:
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        ++flushes;
        return {};
    }
    void shutdown() override {}
    hal::FrameBuffer last{0, 0, {}};
    int flushes{0};
};

/// Forwards to the real compositor and records every rectangle the shell asks it to redraw.
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

bool contains(const Rect& outer, const Rect& inner) {
    return inner.x >= outer.x && inner.y >= outer.y && static_cast<int64_t>(inner.x) + inner.width <= static_cast<int64_t>(outer.x) + outer.width &&
           static_cast<int64_t>(inner.y) + inner.height <= static_cast<int64_t>(outer.y) + outer.height;
}
bool inside(const Rect& r, int64_t x, int64_t y) {
    return x >= r.x && y >= r.y && x < static_cast<int64_t>(r.x) + r.width && y < static_cast<int64_t>(r.y) + r.height;
}

// Explorer's layout (basic_file_explorer.cpp): list rows start at y = 50, 30 px each; the selection highlight spans
// x = 10 .. width - 10 in colour (100, 100, 180); the "Up" button is at (10, 10, 40, 30).
constexpr int32_t kRowTop = 50, kRowHeight = 30;
constexpr uint8_t kHighlight[3] = {100, 100, 180};

struct Desktop {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>();
    std::shared_ptr<RecordingCompositor> compositor = std::make_shared<RecordingCompositor>(std::make_shared<BasicCompositor>(display, logger));
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<BasicWindowDecorator> decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    std::shared_ptr<BasicTaskbar> taskbar = std::make_shared<BasicTaskbar>(toolkit);
    std::shared_ptr<core::MemoryFileSystem> vfs = std::make_shared<core::MemoryFileSystem>();
    apps::ApplicationRegistry registry; // owns the Explorer; declared before the shell, so the shell goes first
    std::unique_ptr<apps::DesktopShell> shell;
    apps::BasicFileExplorer* explorer{nullptr};
    uint32_t win{0};

    Desktop() {
        // Root: docs/, music/, notes.md, readme.txt, todo.txt  (directories first, then names)  -> rows 0..4
        CHECK(vfs->create_directory("/docs"));
        CHECK(vfs->create_directory("/music"));
        for (const char* f : {"/notes.md", "/readme.txt", "/todo.txt", "/docs/a.txt", "/docs/b.txt"}) CHECK(vfs->create(f) != nullptr);

        shell = std::make_unique<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        shell->launch();

        // Launch File Explorer the way CompositionRoot does: through the registry, then host it in a window.
        registry.register_application("BasicFileExplorer", [fs = vfs, tk = toolkit]() { return std::make_unique<apps::BasicFileExplorer>(fs, tk); });
        const bool launched = registry.launch("BasicFileExplorer");
        CHECK(launched);
        const auto running = registry.get_running_applications();
        CHECK(running.size() == 1);
        explorer = dynamic_cast<apps::BasicFileExplorer*>(running.back());
        CHECK(explorer != nullptr);
        CHECK(explorer->get_entries().size() == 5);

        const WindowGeometry geom{200, 200, 700, 500};
        win = wm->create_window("File Explorer", geom);
        auto surface = compositor->create_surface(geom.width, geom.height);
        wm->set_window_surface(win, surface);
        RunningApplication app;
        app.app = explorer;
        app.window_id = win;
        app.surface = surface;
        app.visible = true;
        shell->add_running_application(std::move(app));

        settle();
        shell->focus_window(win); // a click on an unfocused window repaints its frame anyway; test the focused case
        settle();
        compositor->invalidated.clear();
    }

    void frame() {
        shell->update_frame();
        const auto rendered = compositor->render_frame(wm->build_scene());
        CHECK(rendered.has_value());
    }
    void settle() {
        for (int i = 0; i < 3; ++i) frame();
    }
    WindowGeometry client() const { return *wm->get_window_geometry(win); }

    /// Clicks the application at a point in its own (client-area) coordinates, through the real shell input path.
    void click_app(int32_t local_x, int32_t local_y) {
        const auto c = client();
        const int32_t sx = c.x + local_x, sy = c.y + local_y;
        auto send = [&](uint8_t buttons) {
            auto ev = std::make_shared<core::Event>();
            ev->topic = hal::kTopicInputMouse;
            ev->payload = hal::MouseEvent(sx, sy, buttons);
            bus->dispatch_immediate(ev);
        };
        send(1);
        send(0);
        settle();
    }
    void click_row(int row) { click_app(60, kRowTop + kRowHeight * row + kRowHeight / 2); }

    /// Screen rectangle of a list row (the full width of the client area).
    Rect row_strip(int row) const {
        const auto c = client();
        return {c.x, c.y + kRowTop + kRowHeight * row, c.width, static_cast<uint32_t>(kRowHeight)};
    }
    /// Screen rectangle the selection highlight of a row covers.
    Rect row_highlight(int row) const {
        const auto c = client();
        return {c.x + 10, c.y + kRowTop + kRowHeight * row, c.width - 20, static_cast<uint32_t>(kRowHeight)};
    }

    /// The surface the compositor shows for the Explorer's content (the last layer of its window in the scene). The
    /// shell replaces it when the client area changes size.
    std::shared_ptr<ISurface> content_surface() const {
        std::shared_ptr<ISurface> s;
        for (const auto& layer : wm->build_scene()) {
            if (layer.window_id == win) s = layer.surface;
        }
        CHECK(s != nullptr);
        return s;
    }
    std::vector<uint8_t> surface_pixels() const {
        const auto s = content_surface();
        auto px = s->map_pixels();
        std::vector<uint8_t> v(px.begin(), px.end());
        s->unmap_pixels();
        return v;
    }
    /// The composed frame's pixels over the client area, row by row (same layout as the surface).
    std::vector<uint8_t> composed_client() const {
        const auto c = client();
        const auto& fb = display->last;
        std::vector<uint8_t> v;
        v.reserve(static_cast<size_t>(c.width) * c.height * 4);
        for (uint32_t y = 0; y < c.height; ++y) {
            const size_t row = (static_cast<size_t>(c.y + static_cast<int32_t>(y)) * fb.width + static_cast<size_t>(c.x)) * 4;
            v.insert(v.end(), fb.pixels.begin() + static_cast<std::ptrdiff_t>(row), fb.pixels.begin() + static_cast<std::ptrdiff_t>(row + c.width * 4));
        }
        return v;
    }
    int highlight_pixels_on_screen(const Rect& r) const {
        const auto& fb = display->last;
        int n = 0;
        for (int64_t y = r.y; y < static_cast<int64_t>(r.y) + r.height; ++y) {
            for (int64_t x = r.x; x < static_cast<int64_t>(r.x) + r.width; ++x) {
                const size_t i = (static_cast<size_t>(y) * fb.width + static_cast<size_t>(x)) * 4;
                n += (fb.pixels[i] == kHighlight[0] && fb.pixels[i + 1] == kHighlight[1] && fb.pixels[i + 2] == kHighlight[2]) ? 1 : 0;
            }
        }
        return n;
    }

    /// Whether a row shows its selection highlight on screen (most of the highlight area carries the colour; the label
    /// is drawn over the rest).
    bool row_lit(int row) const {
        const Rect r = row_highlight(row);
        return highlight_pixels_on_screen(r) > static_cast<int>(r.width * r.height * 7 / 10);
    }

    /// The core invariant: what is on screen over the client area is exactly what the application rendered.
    void expect_screen_shows_surface() {
        const auto composed = composed_client();
        const auto own = surface_pixels();
        CHECK(composed.size() == own.size());
        size_t first = composed.size();
        for (size_t i = 0; i < composed.size(); ++i) {
            if (composed[i] != own[i]) {
                first = i;
                break;
            }
        }
        if (first != composed.size()) {
            const auto c = client();
            vynex_test::context() += " | stale composed pixel at client (" + std::to_string((first / 4) % c.width) + "," +
                                     std::to_string((first / 4) / c.width) + ")";
        }
        CHECK(first == composed.size());
    }

    /// Every pixel that changed between two frames lies inside one of `allowed`; returns how many changed.
    static int changed_pixels_only_within(const hal::FrameBuffer& before, const hal::FrameBuffer& after, std::initializer_list<Rect> allowed) {
        CHECK(before.pixels.size() == after.pixels.size());
        int changed = 0;
        for (size_t i = 0; i < before.pixels.size(); i += 4) {
            if (before.pixels[i] == after.pixels[i] && before.pixels[i + 1] == after.pixels[i + 1] && before.pixels[i + 2] == after.pixels[i + 2] &&
                before.pixels[i + 3] == after.pixels[i + 3]) {
                continue;
            }
            const int64_t x = static_cast<int64_t>((i / 4) % before.width), y = static_cast<int64_t>((i / 4) / before.width);
            const bool ok = std::any_of(allowed.begin(), allowed.end(), [&](const Rect& r) { return inside(r, x, y); });
            if (!ok) vynex_test::context() += " | pixel changed outside the expected rows at (" + std::to_string(x) + "," + std::to_string(y) + ")";
            CHECK(ok);
            ++changed;
        }
        return changed;
    }

    /// The compositor was asked to redraw only inside `allowed` (ignoring the shell's 16x16 pointer patches).
    void expect_invalidations_within(std::initializer_list<Rect> allowed) const {
        for (const Rect& r : compositor->invalidated) {
            if (r.width == 16 && r.height == 16) continue; // pointer patch, sent for every mouse event
            const bool ok = std::any_of(allowed.begin(), allowed.end(), [&](const Rect& a) { return contains(a, r); });
            if (!ok) vynex_test::context() += " | redraw requested outside the expected rows: (" + std::to_string(r.x) + "," + std::to_string(r.y) + " " +
                                              std::to_string(r.width) + "x" + std::to_string(r.height) + ")";
            CHECK(ok);
        }
    }
    bool any_content_invalidation() const {
        return std::any_of(compositor->invalidated.begin(), compositor->invalidated.end(), [](const Rect& r) { return !(r.width == 16 && r.height == 16); });
    }
};

// ---------------------------------------------------------------------------------------------------------------------
void test_selection_reaches_the_screen() {
    Desktop d;
    vynex_test::context() = "initial state";
    CHECK(!d.explorer->get_selected_index().has_value());
    d.expect_screen_shows_surface();
    CHECK(d.highlight_pixels_on_screen(d.row_highlight(0)) == 0);

    const hal::FrameBuffer before = d.display->last;
    d.compositor->invalidated.clear();
    d.click_row(0);

    vynex_test::context() = "after selecting row 0";
    CHECK(d.explorer->get_selected_index() == std::optional<size_t>{0}); // 1. application state changed
    // 2. the application rendered it: most of the row's highlight area carries the highlight colour in its surface
    {
        const auto own = d.surface_pixels();
        int n = 0;
        for (int y = kRowTop; y < kRowTop + kRowHeight; ++y) {
            for (int x = 10; x < 690; ++x) {
                const size_t i = (static_cast<size_t>(y) * 700 + static_cast<size_t>(x)) * 4;
                n += (own[i] == kHighlight[0] && own[i + 1] == kHighlight[1] && own[i + 2] == kHighlight[2]) ? 1 : 0;
            }
        }
        CHECK(n > 15000);
        // 3. ...and the composed frame shows exactly the same pixels
        CHECK(d.highlight_pixels_on_screen(d.row_highlight(0)) == n);
    }
    d.expect_screen_shows_surface(); // nothing stale anywhere in the window
    // 4. the change stayed local: only the row changed on screen, and only that row was redrawn
    const int changed = Desktop::changed_pixels_only_within(before, d.display->last, {d.row_highlight(0)});
    CHECK(changed > 15000);
    CHECK(d.any_content_invalidation());
    d.expect_invalidations_within({d.row_strip(0)});
    std::printf("test_selection_reaches_the_screen passed (%d pixels changed, all inside row 0)\n", changed);
}

void test_repeated_selection_changes_leave_nothing_stale() {
    Desktop d;
    struct Step {
        const char* what;
        int click_row;                 // row to click (5 = below the last entry: clears the selection)
        std::optional<size_t> selected; // expected selection afterwards
    };
    const Step steps[] = {
        {"select row 0", 0, 0}, {"select row 2", 2, 2}, {"select row 4", 4, 4}, {"select row 1", 1, 1},
        {"click below the list (clears the selection)", 5, std::nullopt}, {"select row 3", 3, 3},
    };
    std::optional<size_t> previous;
    for (const Step& s : steps) {
        vynex_test::context() = std::string("step '") + s.what + "'";
        const hal::FrameBuffer before = d.display->last;
        d.compositor->invalidated.clear();
        d.click_row(s.click_row);

        CHECK(d.explorer->get_selected_index() == s.selected);
        d.expect_screen_shows_surface();
        for (int row = 0; row < 5; ++row) {
            const bool lit = s.selected.has_value() && static_cast<int>(*s.selected) == row;
            CHECK((d.highlight_pixels_on_screen(d.row_highlight(row)) > 15000) == lit); // exactly the selected row is lit
        }
        // Only the previously and newly selected rows may change, and only they may be redrawn.
        const Rect none{0, 0, 0, 0};
        const Rect old_row = previous ? d.row_highlight(static_cast<int>(*previous)) : none;
        const Rect new_row = s.selected ? d.row_highlight(static_cast<int>(*s.selected)) : none;
        (void)Desktop::changed_pixels_only_within(before, d.display->last, {old_row, new_row});
        const Rect old_strip = previous ? d.row_strip(static_cast<int>(*previous)) : none;
        const Rect new_strip = s.selected ? d.row_strip(static_cast<int>(*s.selected)) : none;
        d.expect_invalidations_within({old_strip, new_strip});
        previous = s.selected;
    }
    std::printf("test_repeated_selection_changes_leave_nothing_stale passed (%zu steps)\n", std::size(steps));
}

void test_navigation_repaints_the_list() {
    Desktop d;
    vynex_test::context() = "open the docs directory (select row 0, then activate it)";
    d.click_row(0);
    d.click_row(0);
    CHECK(d.explorer->get_current_path().as_string() == "/docs");
    CHECK(d.explorer->get_entries().size() == 2);
    CHECK(!d.explorer->get_selected_index().has_value());
    d.expect_screen_shows_surface(); // the old five rows, the highlight and the old item count are gone from the screen

    vynex_test::context() = "back up to the root with the Up button";
    d.click_app(30, 25);
    CHECK(d.explorer->get_current_path().as_string() == "/");
    CHECK(d.explorer->get_entries().size() == 5);
    d.expect_screen_shows_surface();
    std::printf("test_navigation_repaints_the_list passed\n");
}

void test_idle_frames_do_no_work() {
    Desktop d;
    d.click_row(1);
    d.settle();
    vynex_test::context() = "idle frames after a selection";
    const int flushes = d.display->flushes;
    d.compositor->invalidated.clear();
    for (int i = 0; i < 30; ++i) d.frame();
    CHECK(d.compositor->invalidated.empty()); // nothing changed, so nothing is redrawn...
    CHECK(d.display->flushes == flushes);     // ...or presented
    d.expect_screen_shows_surface();
    std::printf("test_idle_frames_do_no_work passed\n");
}

void test_moved_window_updates_at_its_new_place() {
    Desktop d;
    // Drag the window by its title bar to a new place (the shell repaints the old and new areas).
    const auto c0 = d.client();
    const int32_t gx = c0.x + 200, gy = c0.y - 18;
    auto mouse = [&](int32_t x, int32_t y, uint8_t b) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputMouse;
        ev->payload = hal::MouseEvent(x, y, b);
        d.bus->dispatch_immediate(ev);
    };
    mouse(gx, gy, 1);
    mouse(gx + 300, gy - 50, 1);
    mouse(gx + 300, gy - 50, 0);
    d.settle();
    vynex_test::context() = "window moved";
    CHECK(d.client().x == c0.x + 300 && d.client().y == c0.y - 50);
    d.expect_screen_shows_surface();

    const hal::FrameBuffer before = d.display->last;
    d.compositor->invalidated.clear();
    d.click_row(2);
    vynex_test::context() = "selection in the moved window";
    CHECK(d.explorer->get_selected_index() == std::optional<size_t>{2});
    d.expect_screen_shows_surface();
    (void)Desktop::changed_pixels_only_within(before, d.display->last, {d.row_highlight(2)});
    d.expect_invalidations_within({d.row_strip(2)});
    std::printf("test_moved_window_updates_at_its_new_place passed\n");
}


void test_selection_survives_moving_resizing_and_maximizing() {
    // P-3 and P-4 together on the real Explorer: after every move, resize, maximize and restore the application is the
    // same instance with the same selection, its surface is exactly the client area, the screen shows that surface,
    // exactly the selected row is lit, and clicks select the rows they land on at the new size and place.
    Desktop d;
    vynex_test::Pointer pointer(*d.bus);
    const apps::BasicFileExplorer* const instance = d.explorer;
    auto expect_consistent = [&](std::optional<size_t> selected) {
        CHECK(d.explorer == instance);
        CHECK(d.explorer->get_selected_index() == selected);
        const auto c = d.client();
        const auto shown = d.content_surface();
        CHECK(shown->width() == c.width && shown->height() == c.height);
        d.expect_screen_shows_surface();
        for (int row = 0; row < 5; ++row) {
            const bool lit = selected.has_value() && static_cast<int>(*selected) == row;
            CHECK(d.row_lit(row) == lit);
        }
    };

    vynex_test::context() = "select row 1";
    d.click_row(1);
    expect_consistent(1);

    vynex_test::context() = "move the window";
    const auto grab = vynex_test::title_bar_point(*d.wm, d.win);
    pointer.drag(grab, {grab.x + 120, grab.y + 90});
    d.settle();
    expect_consistent(1);

    struct Resize {
        const char* what;
        ResizeEdge edge;
        int32_t dx, dy;
    };
    const Resize resizes[] = {
        {"resize from the right edge", ResizeEdge::Right, 150, 0},
        {"resize from the bottom edge", ResizeEdge::Bottom, 0, 80},
        {"resize from the left edge", ResizeEdge::Left, -100, 0},
        {"resize from the top edge", ResizeEdge::Top, 0, 30},
        {"resize from the bottom-right corner", ResizeEdge::BottomRight, -380, -200},
        {"resize from the top-left corner", ResizeEdge::TopLeft, -60, -40},
    };
    size_t selected = 1;
    int next = 2;
    for (const Resize& r : resizes) {
        vynex_test::context() = r.what;
        const auto before = d.client();
        const auto from = vynex_test::resize_point(*d.wm, d.win, r.edge);
        pointer.drag(from, {from.x + r.dx, from.y + r.dy});
        d.settle();
        CHECK(d.client().width != before.width || d.client().height != before.height);
        expect_consistent(selected); // resizing neither resets the application nor changes its selection

        vynex_test::context() = std::string(r.what) + ", then select a row at the new size";
        d.click_row(next);
        selected = static_cast<size_t>(next);
        expect_consistent(selected);
        next = (next + 2) % 5;
    }

    vynex_test::context() = "maximize";
    pointer.click(vynex_test::control_point(*d.wm, d.win, DecorationHitRegion::MaximizeButton));
    d.settle();
    CHECK(d.wm->get_window_state(d.win) == WindowState::Maximized);
    expect_consistent(selected);
    d.click_row(4);
    expect_consistent(4);

    vynex_test::context() = "restore";
    const auto maximized = d.client();
    pointer.click(vynex_test::control_point(*d.wm, d.win, DecorationHitRegion::MaximizeButton));
    d.settle();
    CHECK(d.wm->get_window_state(d.win) == WindowState::Normal);
    CHECK(d.client().width < maximized.width);
    expect_consistent(4);
    d.click_row(0);
    expect_consistent(0);
    std::printf("test_selection_survives_moving_resizing_and_maximizing passed\n");
}

void test_dragging_across_rows_is_not_a_click() {
    // A press selects; motion with the button held (within the row, across the others, back) and the release change
    // nothing. Row 0 is a directory: treating any held-button event as another click would open it.
    Desktop d;
    vynex_test::Pointer pointer(*d.bus);
    const auto c = d.client();
    auto at_row = [&](int row, int dx) { return vynex_test::Point{c.x + 60 + dx, c.y + kRowTop + kRowHeight * row + kRowHeight / 2}; };
    const hal::FrameBuffer before = d.display->last;
    d.compositor->invalidated.clear();
    pointer.send(at_row(0, 0), 0); // hover first: nothing
    pointer.send(at_row(0, 0), 1); // press: select
    for (int dx = 1; dx <= 5; ++dx) pointer.send(at_row(0, 7 * dx), 1);
    for (int row = 1; row < 5; ++row) pointer.send(at_row(row, 0), 1);
    pointer.send(at_row(0, 0), 1);
    pointer.send(at_row(0, 0), 0); // release
    for (int row = 0; row < 5; ++row) pointer.send(at_row(row, 3), 0); // free motion afterwards
    d.settle();
    vynex_test::context() = "drag across the rows";
    CHECK(d.explorer->get_current_path().as_string() == "/"); // not opened
    CHECK(d.explorer->get_selected_index() == std::optional<size_t>{0});
    d.expect_screen_shows_surface();
    CHECK(d.row_lit(0) && !d.row_lit(1) && !d.row_lit(2) && !d.row_lit(3) && !d.row_lit(4));
    (void)Desktop::changed_pixels_only_within(before, d.display->last, {d.row_highlight(0)});
    d.expect_invalidations_within({d.row_strip(0)});

    vynex_test::context() = "a real second click still opens it";
    d.click_row(0);
    CHECK(d.explorer->get_current_path().as_string() == "/docs");
    d.expect_screen_shows_surface();
    std::printf("test_dragging_across_rows_is_not_a_click passed\n");
}

} // namespace

int main() {
    std::printf("Running Explorer content presentation tests...\n");
    test_selection_reaches_the_screen();
    test_repeated_selection_changes_leave_nothing_stale();
    test_navigation_repaints_the_list();
    test_idle_frames_do_no_work();
    test_moved_window_updates_at_its_new_place();
    test_selection_survives_moving_resizing_and_maximizing();
    test_dragging_across_rows_is_not_a_click();
std::printf("All Explorer content presentation tests passed.\n");
    return 0;
}
