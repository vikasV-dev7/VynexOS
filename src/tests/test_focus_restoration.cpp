// Focus through the desktop shell and the application host: who has the focus after a close, a minimize and a restore,
// what the taskbar and the desktop do to it, input over the launcher and the taskbar (which are above application
// windows), and where keyboard input goes after each transition. The focus never refers to a window that is closed,
// minimized, hidden, not an application, or anything but the topmost choice.
#include <algorithm>
#include <cstdint>
#include <cstdio>
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

class NullDisplay final : public IDisplayAbstraction {
public:
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer&) override { return {}; }
    void shutdown() override {}
};

/// The real taskbar, recording what the shell last told it (which entry is active).
class RecordingTaskbar final : public ITaskbar {
public:
    explicit RecordingTaskbar(std::shared_ptr<BasicTaskbar> inner) : m_inner(std::move(inner)) {}
    bool synchronize(const std::vector<TaskbarEntry>& entries) override {
        last = entries;
        return m_inner->synchronize(entries);
    }
    void draw(std::shared_ptr<ISurface> surface) override { m_inner->draw(std::move(surface)); }
    std::optional<uint32_t> get_clicked_window(int32_t x, int32_t y) const override { return m_inner->get_clicked_window(x, y); }
    std::vector<TaskbarEntry> last;
    std::optional<uint32_t> active() const {
        std::optional<uint32_t> found;
        for (const auto& e : last) {
            if (e.is_active) {
                CHECK(!found.has_value()); // at most one active entry
                found = e.window_id;
            }
        }
        return found;
    }

private:
    std::shared_ptr<BasicTaskbar> m_inner;
};

class ProbeApp final : public apps::IApplication {
public:
    int mouse_events{0};
    std::vector<std::pair<uint32_t, bool>> keys;
    bool initialize() override { return true; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event& ev) override {
        if (ev.payload.type() == typeid(hal::MouseEvent)) ++mouse_events;
        if (ev.payload.type() == typeid(hal::KeyEvent)) {
            const auto k = std::any_cast<hal::KeyEvent>(ev.payload);
            keys.emplace_back(k.key_code, k.pressed);
        }
        return true;
    }
    void render(ISurface& s) override {
        auto px = s.map_pixels();
        std::fill(px.begin(), px.end(), uint8_t{90});
        s.unmap_pixels();
    }
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "Probe"; }
};

struct Rig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<BasicCompositor> compositor = std::make_shared<BasicCompositor>(std::make_shared<NullDisplay>(), logger);
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<RecordingTaskbar> taskbar = std::make_shared<RecordingTaskbar>(std::make_shared<BasicTaskbar>(toolkit));
    std::shared_ptr<apps::ApplicationRegistry> registry = std::make_shared<apps::ApplicationRegistry>();
    std::shared_ptr<apps::DesktopShell> shell;
    std::shared_ptr<apps::ApplicationHost> host;
    vynex_test::Pointer pointer{*bus};
    uint32_t launcher{0}, taskbar_window{0}, wallpaper{0};

    // Launcher order: 0 A, 1 B, 2 C (overlapping), 3 Low (reaches under the taskbar), 4 UnderLauncher.
    Rig() {
        shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, std::make_shared<BasicWindowDecorator>(toolkit), taskbar, bus);
        host = std::make_shared<apps::ApplicationHost>(registry, wm, compositor, shell);
        shell->set_application_host(host.get());
        auto probe = [] { return std::make_unique<ProbeApp>(); };
        host->register_application({"A", "A", {300, 150, 400, 300}}, probe);
        host->register_application({"B", "B", {500, 250, 400, 300}}, probe);
        host->register_application({"C", "C", {700, 350, 400, 300}}, probe);
        host->register_application({"Low", "Low", {1300, 900, 400, 300}}, probe);
        host->register_application({"UnderLauncher", "Under the launcher", {50, 520, 500, 300}}, probe); // under the launcher's first entry
        shell->launch();
        for (const auto& s : wm->get_window_snapshots()) {
            if (s.title == "Launcher") launcher = s.window_id;
            if (s.title == "Taskbar") taskbar_window = s.window_id;
            if (s.title == "Wallpaper") wallpaper = s.window_id;
        }
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
    ProbeApp* probe(uint32_t window) const { return dynamic_cast<ProbeApp*>(host->application_in(window)); }
    Point content(uint32_t window, int32_t x = 50, int32_t y = 50) const {
        const auto c = *wm->get_window_geometry(window);
        return {c.x + x, c.y + y};
    }
    void click(Point p) {
        pointer.click(p);
        settle();
    }
    void close_button(uint32_t window) { click(vynex_test::control_point(*wm, window, DecorationHitRegion::CloseButton)); }
    void minimize_button(uint32_t window) { click(vynex_test::control_point(*wm, window, DecorationHitRegion::MinimizeButton)); }
    void maximize_button(uint32_t window) { click(vynex_test::control_point(*wm, window, DecorationHitRegion::MaximizeButton)); }
    /// A point on the taskbar button of `window`.
    Point taskbar_button(uint32_t window) const {
        const auto t = *wm->get_window_geometry(taskbar_window);
        for (int32_t x = 0; x < static_cast<int32_t>(t.width); x += 5) {
            if (taskbar->get_clicked_window(x, 20) == std::optional<uint32_t>{window}) return {t.x + x + 10, t.y + 20};
        }
        CHECK(false);
        return {};
    }
    Point empty_taskbar_spot() const {
        const auto t = *wm->get_window_geometry(taskbar_window);
        const Point p{t.x + static_cast<int32_t>(t.width) - 300, t.y + 20};
        CHECK(!taskbar->get_clicked_window(p.x - t.x, p.y - t.y).has_value());
        return p;
    }
    void toggle_launcher() { click({50, static_cast<int32_t>(compositor->logical_size().height) - 20}); }
    Point launcher_entry(size_t i) const {
        const auto g = *wm->get_window_geometry(launcher);
        return {g.x + 150, g.y + 50 + static_cast<int32_t>(i) * 50 + 20};
    }

    /// The topmost application window (the Normal layer) that is visible and not minimized.
    std::optional<uint32_t> top_application() const {
        const auto stack = wm->get_visible_windows_z_ordered();
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (wm->get_window_layer(*it) == WindowLayer::Normal) return *it;
        }
        return std::nullopt;
    }
    /// The focus refers to an open, visible, not minimized application window, and the taskbar shows the same one.
    void expect_focus_valid() {
        const auto f = shell->focused_window();
        CHECK(taskbar->active() == f);
        if (!f) return;
        const auto visible = wm->get_visible_windows_z_ordered();
        CHECK(std::find(visible.begin(), visible.end(), *f) != visible.end()); // exists, shown, not minimized
        CHECK(wm->get_window_layer(*f) == WindowLayer::Normal);
        CHECK(host->application_in(*f) != nullptr);
    }
    /// Taps a key and returns which window's application received it (0: none). At most one may.
    uint32_t key_receiver(uint32_t code = 'k') {
        std::vector<std::pair<uint32_t, size_t>> before;
        for (const auto& s : wm->get_window_snapshots()) {
            if (auto* p = probe(s.window_id)) before.emplace_back(s.window_id, p->keys.size());
        }
        for (const bool pressed : {true, false}) {
            auto ev = std::make_shared<core::Event>();
            ev->topic = hal::kTopicInputKey;
            ev->payload = hal::KeyEvent(code, pressed);
            bus->dispatch_immediate(ev);
        }
        uint32_t receiver = 0;
        for (const auto& [window, count] : before) {
            const size_t got = probe(window)->keys.size() - count;
            if (got == 0) continue;
            CHECK(got == 2 && receiver == 0); // press and release, to one application only
            receiver = window;
        }
        return receiver;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
void test_focus_after_close() {
    Rig r;
    const uint32_t a = r.open("A"), b = r.open("B"), c = r.open("C");
    vynex_test::context() = "the last opened window has the focus";
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{c} && r.top_application() == std::optional<uint32_t>{c});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == c);

    vynex_test::context() = "closing the focused window focuses the next one in the stack";
    r.close_button(c);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{b});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == b);

    vynex_test::context() = "closing an unfocused window keeps the focus";
    r.click(r.content(a));
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a});
    CHECK(r.host->close(b));
    r.settle();
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == a);

    vynex_test::context() = "closing the last window leaves no focus and the keyboard goes nowhere";
    r.close_button(a);
    CHECK(!r.shell->focused_window().has_value());
    r.expect_focus_valid();
    CHECK(r.key_receiver() == 0);

    vynex_test::context() = "closing through the host (not the button) also hands the focus on";
    const uint32_t a2 = r.open("A"), b2 = r.open("B");
    CHECK(r.host->close(b2));
    r.settle();
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a2});
    r.expect_focus_valid();

    vynex_test::context() = "a key pressed before the focused window closes is not released into the next one";
    const uint32_t c2 = r.open("C");
    auto key = [&](bool pressed) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputKey;
        ev->payload = hal::KeyEvent('q', pressed);
        r.bus->dispatch_immediate(ev);
    };
    key(true);
    const size_t a2_keys = r.probe(a2)->keys.size();
    r.close_button(c2);
    key(false);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a2});
    CHECK(r.probe(a2)->keys.size() == a2_keys); // its press went to the closed window
    CHECK(r.key_receiver() == a2);
    std::printf("test_focus_after_close passed\n");
}

void test_focus_after_minimize_and_restore() {
    Rig r;
    const uint32_t a = r.open("A"), b = r.open("B"), c = r.open("C");

    vynex_test::context() = "minimizing the focused window focuses the next one";
    r.minimize_button(c);
    CHECK(r.wm->get_window_state(c) == WindowState::Minimized);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{b});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == b);

    vynex_test::context() = "minimizing through the taskbar button too";
    r.click(r.taskbar_button(b));
    CHECK(r.wm->get_window_state(b) == WindowState::Minimized);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == a);

    vynex_test::context() = "restoring through the taskbar focuses and raises the window";
    r.click(r.taskbar_button(c));
    CHECK(r.wm->get_window_state(c) == WindowState::Normal);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{c} && r.top_application() == std::optional<uint32_t>{c});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == c);

    vynex_test::context() = "restoring by opening it again (launcher policy) focuses it";
    CHECK(r.host->open("B") == std::optional<uint32_t>{b});
    r.settle();
    CHECK(r.wm->get_window_state(b) == WindowState::Normal);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{b} && r.top_application() == std::optional<uint32_t>{b});
    r.expect_focus_valid();

    vynex_test::context() = "minimizing everything leaves no focus; a minimized window never has it";
    for (int i = 0; i < 3; ++i) {
        const auto f = r.shell->focused_window();
        CHECK(f.has_value());
        r.minimize_button(*f);
        r.expect_focus_valid();
        if (const auto next = r.shell->focused_window()) CHECK(next == r.top_application());
    }
    CHECK(!r.shell->focused_window().has_value() && !r.top_application().has_value());
    CHECK(r.key_receiver() == 0);

    vynex_test::context() = "restore, then maximize and restore: geometry back, focus kept";
    r.click(r.taskbar_button(a));
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a});
    const auto normal = *r.wm->get_window_geometry(a);
    r.maximize_button(a);
    CHECK(r.wm->get_window_state(a) == WindowState::Maximized && r.shell->focused_window() == std::optional<uint32_t>{a});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == a);
    r.maximize_button(a);
    const auto back = *r.wm->get_window_geometry(a);
    CHECK(back.x == normal.x && back.y == normal.y && back.width == normal.width && back.height == normal.height);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a});
    r.expect_focus_valid();
    std::printf("test_focus_after_minimize_and_restore passed\n");
}

void test_taskbar_and_desktop_focus_policy() {
    Rig r;
    const uint32_t a = r.open("A"), b = r.open("B");

    vynex_test::context() = "an empty spot on the taskbar does not change the focus";
    r.click(r.empty_taskbar_spot());
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{b});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == b);

    vynex_test::context() = "the desktop takes the focus away from every window";
    r.click({1750, 100});
    CHECK(r.wm->get_window_at(1750, 100) == std::optional<uint32_t>{r.wallpaper});
    CHECK(!r.shell->focused_window().has_value());
    r.expect_focus_valid(); // the taskbar shows no active entry either
    CHECK(r.key_receiver() == 0);

    vynex_test::context() = "a taskbar button focuses its window";
    r.click(r.taskbar_button(a));
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a} && r.top_application() == std::optional<uint32_t>{a});
    r.expect_focus_valid();
    CHECK(r.key_receiver() == a);
    std::printf("test_taskbar_and_desktop_focus_policy passed\n");
}

void test_launcher_and_panel_input_over_windows() {
    Rig r;
    const uint32_t under = r.open("UnderLauncher");
    const auto stack_before = r.wm->get_windows_z_ordered();

    vynex_test::context() = "the launcher shows above the windows without being raised or focused";
    r.toggle_launcher();
    CHECK(r.wm->is_window_visible(r.launcher));
    const auto scene = r.wm->build_scene();
    CHECK(!scene.empty() && scene.back().window_id == r.launcher); // composed last: on top
    {
        // The launcher is the token size and the Overlay elevation: surface_overlay (#2A333D) inside a one-pixel
        // border (#36414D), so it stands out from the window bodies (#1A2027) it lies over.
        const auto& panel = scene.back().surface;
        CHECK(panel->width() == 420 && panel->height() == 520);
        auto lp = panel->map_pixels();
        auto at = [&](uint32_t x, uint32_t y) { return (static_cast<size_t>(y) * 420 + x) * 4; };
        for (const size_t i : {at(0, 0), at(419, 0), at(0, 519), at(419, 519), at(200, 519), at(419, 300)}) {
            CHECK(lp[i] == 0x36 && lp[i + 1] == 0x41 && lp[i + 2] == 0x4D && lp[i + 3] == 255);
        }
        const size_t inside = at(200, 400); // below the entries
        CHECK(lp[inside] == 0x2A && lp[inside + 1] == 0x33 && lp[inside + 2] == 0x3D && lp[inside + 3] == 255);
        panel->unmap_pixels();
    }
    CHECK(r.wm->get_windows_z_ordered() == stack_before);          // nothing was restacked
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{under});
    CHECK(r.wm->get_active_window() != std::optional<uint32_t>{r.launcher}); // not activated in the window manager either
    const Point entry = r.launcher_entry(0);
    const auto uc = *r.wm->get_window_geometry(under);
    CHECK(client_contains(uc, entry.x, entry.y)); // the entry lies over the window's content
    CHECK(r.wm->get_window_at(entry.x, entry.y) == std::optional<uint32_t>{r.launcher});

    vynex_test::context() = "a click on the entry opens the application; the window beneath sees nothing";
    r.probe(under)->mouse_events = 0;
    r.click(entry);
    const auto a = r.host->window_of("A");
    CHECK(a.has_value());
    CHECK(r.probe(under)->mouse_events == 0);
    CHECK(!r.wm->is_window_visible(r.launcher));
    CHECK(r.shell->focused_window() == a);
    r.expect_focus_valid();

    vynex_test::context() = "the taskbar is above a window that reaches under it";
    const uint32_t low = r.open("Low");
    const Point spot{1500, static_cast<int32_t>(r.compositor->logical_size().height) - 20};
    CHECK(client_contains(*r.wm->get_window_geometry(low), spot.x, spot.y));
    CHECK(r.wm->get_window_at(spot.x, spot.y) == std::optional<uint32_t>{r.taskbar_window});
    r.probe(low)->mouse_events = 0;
    r.click(spot);
    CHECK(r.probe(low)->mouse_events == 0);
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{low}); // and the taskbar did not take the focus
    r.expect_focus_valid();

    vynex_test::context() = "a press that began in the window stays captured over the taskbar";
    r.probe(low)->mouse_events = 0;
    r.pointer.send(r.content(low, 100, 50), 1);
    r.pointer.send(spot, 1);
    r.pointer.send(spot, 0);
    r.settle();
    CHECK(r.probe(low)->mouse_events == 3);
    std::printf("test_launcher_and_panel_input_over_windows passed\n");
}

} // namespace

int main() {
    std::printf("Running focus restoration tests...\n");
    test_focus_after_close();
    test_focus_after_minimize_and_restore();
    test_taskbar_and_desktop_focus_policy();
    test_launcher_and_panel_input_over_windows();
    std::printf("All focus restoration tests passed.\n");
    return 0;
}
