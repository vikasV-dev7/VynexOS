// Application lifecycle through the real desktop shell, window manager, compositor, application registry and
// application host: launching from the launcher, the single-instance policy, closing (window, instance and surface go
// together) and reopening, keyboard and text input following focus, the Terminal as a real application, and the
// release an application receives when its pointer interaction is cancelled.
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../apps/application_host.hpp"
#include "../apps/basic_terminal.hpp"
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

/// Draws with the real toolkit and records every text it is asked to draw.
class RecordingToolkit final : public IWidgetToolkit {
public:
    struct Text {
        int32_t x, y;
        std::string text;
    };
    explicit RecordingToolkit(std::shared_ptr<BasicWidgetToolkit> inner) : m_inner(std::move(inner)) {}
    void draw_rect(std::shared_ptr<ISurface> s, int32_t x, int32_t y, uint32_t w, uint32_t h, Color c) override { m_inner->draw_rect(s, x, y, w, h, c); }
    void draw_text(std::shared_ptr<ISurface> s, int32_t x, int32_t y, std::string_view text, Color c) override {
        texts.push_back({x, y, std::string(text)});
        m_inner->draw_text(s, x, y, text, c);
    }
    void draw_button(std::shared_ptr<ISurface> s, int32_t x, int32_t y, uint32_t w, uint32_t h, std::string_view label) override {
        m_inner->draw_button(s, x, y, w, h, label);
    }
    void draw_panel(std::shared_ptr<ISurface> s, int32_t x, int32_t y, uint32_t w, uint32_t h) override {
        texts.clear(); // a panel starts a new paint of the whole content
        m_inner->draw_panel(s, x, y, w, h);
    }
    [[nodiscard]] const Theme& theme() const override { return m_inner->theme(); }
    std::vector<Text> texts; // since the last panel

private:
    std::shared_ptr<BasicWidgetToolkit> m_inner;
};

/// Records its lifecycle and every event it receives; paints its whole surface in one colour.
struct ProbeCounters {
    int alive{0};
    int created{0};
    int shutdowns{0};
};
class ProbeApp final : public apps::IApplication {
public:
    struct Received {
        std::string topic;
        int32_t x{0}, y{0};
        uint32_t code{0};
        bool pressed{false};
        uint8_t buttons{0};
        std::string text;
    };
    ProbeApp(ProbeCounters& counters, uint8_t colour) : m_counters(counters), m_colour(colour) {
        ++m_counters.alive;
        ++m_counters.created;
    }
    ~ProbeApp() override { --m_counters.alive; }
    bool initialize() override { return true; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event& ev) override {
        Received r{ev.topic};
        if (ev.payload.type() == typeid(hal::MouseEvent)) {
            const auto m = std::any_cast<hal::MouseEvent>(ev.payload);
            r.x = m.x, r.y = m.y, r.buttons = m.button_state;
        } else if (ev.payload.type() == typeid(hal::KeyEvent)) {
            const auto k = std::any_cast<hal::KeyEvent>(ev.payload);
            r.code = k.key_code, r.pressed = k.pressed;
        } else if (ev.payload.type() == typeid(hal::TextInputEvent)) {
            r.text = std::any_cast<hal::TextInputEvent>(ev.payload).text;
        }
        received.push_back(r);
        return true;
    }
    void render(ISurface& s) override {
        ++renders;
        auto px = s.map_pixels();
        for (size_t i = 0; i < px.size(); i += 4) px[i] = m_colour, px[i + 1] = 60, px[i + 2] = 90, px[i + 3] = 255;
        s.unmap_pixels();
    }
    void shutdown() override { ++m_counters.shutdowns; }
    [[nodiscard]] std::string get_name() const noexcept override { return "Probe"; }

    std::vector<Received> received;
    int renders{0};
    std::vector<Received> keyboard() const {
        std::vector<Received> k;
        for (const auto& r : received) {
            if (r.topic != hal::kTopicInputMouse) k.push_back(r);
        }
        return k;
    }

private:
    ProbeCounters& m_counters;
    uint8_t m_colour;
};

struct Rig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>();
    std::shared_ptr<RecordingCompositor> compositor = std::make_shared<RecordingCompositor>(std::make_shared<BasicCompositor>(display, logger));
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<RecordingToolkit> terminal_toolkit = std::make_shared<RecordingToolkit>(toolkit);
    std::shared_ptr<BasicWindowDecorator> decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    std::shared_ptr<BasicTaskbar> taskbar = std::make_shared<BasicTaskbar>(toolkit);
    std::shared_ptr<apps::ApplicationRegistry> registry = std::make_shared<apps::ApplicationRegistry>();
    std::shared_ptr<apps::DesktopShell> shell;
    std::shared_ptr<apps::ApplicationHost> host;
    vynex_test::Pointer pointer{*bus};
    ProbeCounters counters_a, counters_b;
    uint32_t launcher{0};

    // Launcher order: 0 Probe A, 1 Probe B, 2 Terminal.
    Rig() {
        shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        host = std::make_shared<apps::ApplicationHost>(registry, wm, compositor, shell);
        shell->set_application_host(host.get());
        host->register_application({"ProbeA", "Probe A", {300, 200, 400, 300}}, [this]() { return std::make_unique<ProbeApp>(counters_a, uint8_t{40}); });
        host->register_application({"ProbeB", "Probe B", {900, 250, 400, 300}}, [this]() { return std::make_unique<ProbeApp>(counters_b, uint8_t{140}); });
        host->register_application({"Terminal", "Terminal", {100, 450, 600, 400}},
                                   [this]() { return std::make_unique<apps::BasicTerminal>(terminal_toolkit); });
        shell->launch();
        for (const auto& s : wm->get_window_snapshots()) {
            if (s.title == "Launcher") launcher = s.window_id;
        }
        CHECK(launcher != 0);
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

    bool launcher_visible() const { return wm->is_window_visible(launcher); }
    void toggle_launcher() {
        pointer.click({50, static_cast<int32_t>(compositor->logical_size().height) - 20}); // the Vynex button
        settle();
    }
    Point entry_point(size_t index) const {
        const auto g = *wm->get_window_geometry(launcher);
        return {g.x + 150, g.y + 50 + static_cast<int32_t>(index) * 50 + 20};
    }
    /// Opens the launcher (if closed) and clicks an entry.
    void launch_from_launcher(size_t index) {
        if (!launcher_visible()) toggle_launcher();
        CHECK(launcher_visible());
        pointer.click(entry_point(index));
        settle();
    }
    void key(uint32_t code, bool pressed) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputKey;
        ev->payload = hal::KeyEvent(code, pressed);
        bus->dispatch_immediate(ev);
    }
    void tap(uint32_t code) {
        key(code, true);
        key(code, false);
    }
    void type(const std::string& text) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputText;
        ev->payload = hal::TextInputEvent{text};
        bus->dispatch_immediate(ev);
    }
    ProbeApp* probe(std::string_view app_id) const {
        const auto w = host->window_of(app_id);
        return w ? dynamic_cast<ProbeApp*>(host->application_in(*w)) : nullptr;
    }
    Point content(uint32_t window, int32_t x, int32_t y) const {
        const auto c = *wm->get_window_geometry(window);
        return {c.x + x, c.y + y};
    }
    std::shared_ptr<ISurface> content_surface(uint32_t window) const {
        std::shared_ptr<ISurface> s;
        for (const auto& layer : wm->build_scene()) {
            if (layer.window_id == window) s = layer.surface;
        }
        return s;
    }
    int layers_of(uint32_t window) const {
        const auto scene = wm->build_scene();
        return static_cast<int>(std::count_if(scene.begin(), scene.end(), [&](const RenderLayer& l) { return l.window_id == window; }));
    }
    /// The screen over the window's client area equals its content surface, which is the client area's size.
    void expect_screen_shows(uint32_t window) const {
        const auto c = *wm->get_window_geometry(window);
        const auto s = content_surface(window);
        CHECK(s && s->width() == c.width && s->height() == c.height);
        const auto& fb = display->last;
        auto px = s->map_pixels();
        size_t mismatch = 0;
        int32_t min_y = INT32_MAX, max_y = INT32_MIN;
        for (uint32_t y = 0; y < c.height; ++y) {
            for (uint32_t x = 0; x < c.width; ++x) {
                const size_t a = (static_cast<size_t>(y) * c.width + x) * 4;
                const size_t b = (static_cast<size_t>(c.y + static_cast<int32_t>(y)) * fb.width + static_cast<size_t>(c.x + static_cast<int32_t>(x))) * 4;
                if (px[a] != fb.pixels[b] || px[a + 1] != fb.pixels[b + 1] || px[a + 2] != fb.pixels[b + 2]) {
                    ++mismatch;
                    min_y = std::min(min_y, static_cast<int32_t>(y));
                    max_y = std::max(max_y, static_cast<int32_t>(y));
                }
            }
        }
        s->unmap_pixels();
        if (mismatch) {
            vynex_test::context() += " | " + std::to_string(mismatch) + " composed pixels differ from the surface, content rows " +
                                     std::to_string(min_y) + ".." + std::to_string(max_y);
        }
        CHECK(mismatch == 0);
    }
    void close(uint32_t window) {
        pointer.click(vynex_test::control_point(*wm, window, DecorationHitRegion::CloseButton));
        settle();
    }
};

// ---------------------------------------------------------------------------------------------------------------------
void test_launcher_opens_applications() {
    Rig r;
    vynex_test::context() = "launcher opened by the Vynex button";
    r.toggle_launcher();
    CHECK(r.launcher_visible());
    CHECK(r.layers_of(r.launcher) == 1); // a desktop component: its surface only, no decorations
    CHECK(r.wm->get_window_at(r.entry_point(0).x, r.entry_point(0).y) == std::optional<uint32_t>{r.launcher});

    vynex_test::context() = "press on one entry, release on another: nothing happens";
    r.pointer.send(r.entry_point(0), 1);
    r.pointer.send(r.entry_point(1), 1);
    r.pointer.send(r.entry_point(1), 0);
    r.settle();
    CHECK(!r.host->window_of("ProbeA") && !r.host->window_of("ProbeB"));
    CHECK(r.registry->get_running_applications().empty());
    CHECK(r.launcher_visible());
    // Pressing the launcher neither focuses it nor gives it a window frame.
    CHECK(r.shell->focused_window() != std::optional<uint32_t>{r.launcher});
    CHECK(r.layers_of(r.launcher) == 1);

    vynex_test::context() = "Escape dismisses the launcher";
    r.tap(hal::kKeyEscape);
    r.settle();
    CHECK(!r.launcher_visible());
    r.toggle_launcher();
    vynex_test::context() = "a press elsewhere dismisses the launcher";
    r.pointer.click({1500, 100});
    r.settle();
    CHECK(!r.launcher_visible());

    vynex_test::context() = "clicking the Terminal entry";
    r.launch_from_launcher(2);
    const auto terminal = r.host->window_of("Terminal");
    CHECK(terminal.has_value());
    CHECK(!r.launcher_visible());                                   // launching closes the launcher
    CHECK(r.shell->focused_window() == terminal);                   // the new window has the focus
    CHECK(r.registry->get_running_applications().size() == 1);
    CHECK(dynamic_cast<apps::BasicTerminal*>(r.host->application_in(*terminal)) != nullptr);
    CHECK(r.layers_of(r.launcher) == 0);                            // hidden, and it never received decorations
    r.expect_screen_shows(*terminal);
    std::printf("test_launcher_opens_applications passed\n");
}

void test_single_instance_close_and_reopen() {
    Rig r;
    r.launch_from_launcher(0);
    const auto first = r.host->window_of("ProbeA");
    CHECK(first.has_value());
    ProbeApp* const instance = r.probe("ProbeA");
    CHECK(instance && instance->renders == 1);
    r.expect_screen_shows(*first);

    vynex_test::context() = "opening it again brings the same window forward";
    r.launch_from_launcher(1); // Probe B on top
    r.launch_from_launcher(0);
    CHECK(r.host->window_of("ProbeA") == first && r.probe("ProbeA") == instance);
    CHECK(r.counters_a.alive == 1 && r.counters_a.created == 1);
    CHECK(r.shell->focused_window() == first);
    CHECK(r.wm->get_windows_z_ordered().back() == *first);

    vynex_test::context() = "a minimized instance is restored";
    r.pointer.click(vynex_test::control_point(*r.wm, *first, DecorationHitRegion::MinimizeButton));
    r.settle();
    CHECK(r.wm->get_window_state(*first) == WindowState::Minimized);
    r.launch_from_launcher(0);
    CHECK(r.wm->get_window_state(*first) == WindowState::Normal);
    CHECK(r.shell->focused_window() == first && r.counters_a.created == 1);
    r.expect_screen_shows(*first);

    vynex_test::context() = "closing ends the instance and removes the window";
    const std::weak_ptr<ISurface> surface = r.content_surface(*first);
    const size_t running = r.shell->running_application_count();
    r.close(*first);
    CHECK(!r.wm->get_window_geometry(*first).has_value());
    CHECK(r.layers_of(*first) == 0);
    CHECK(!r.host->window_of("ProbeA") && r.host->application_in(*first) == nullptr);
    CHECK(r.counters_a.alive == 0 && r.counters_a.shutdowns == 1);
    CHECK(r.shell->running_application_count() == running - 1);
    CHECK(r.registry->get_running_applications().size() == 1); // only Probe B
    CHECK(r.shell->focused_window() != first);
    CHECK(surface.expired()); // nobody keeps the closed window's surface

    vynex_test::context() = "reopened: a new instance in a new window, painted in full";
    r.launch_from_launcher(0);
    const auto second = r.host->window_of("ProbeA");
    CHECK(second.has_value() && *second != *first);
    CHECK(r.counters_a.alive == 1 && r.counters_a.created == 2);
    CHECK(r.probe("ProbeA")->renders == 1);
    r.expect_screen_shows(*second);
    // ...and it follows its window's size like any other (Stage 4).
    auto g = *r.wm->get_window_geometry(*second);
    g.width += 120;
    g.height -= 60;
    r.wm->set_window_geometry(*second, g);
    r.settle();
    CHECK(r.probe("ProbeA")->renders == 2);
    r.expect_screen_shows(*second);

    vynex_test::context() = "repeated open/close cycles";
    const size_t windows = r.wm->get_window_snapshots().size();
    for (int i = 0; i < 10; ++i) {
        r.close(*r.host->window_of("ProbeA"));
        CHECK(r.counters_a.alive == 0);
        CHECK(r.wm->get_window_snapshots().size() == windows - 1);
        r.launch_from_launcher(0);
        CHECK(r.counters_a.alive == 1);
        CHECK(r.wm->get_window_snapshots().size() == windows);
    }
    CHECK(r.counters_a.created == 12 && r.counters_a.shutdowns == 11);
    CHECK(r.registry->get_running_applications().size() == 2);
    std::printf("test_single_instance_close_and_reopen passed\n");
}

void test_keyboard_follows_focus() {
    Rig r;
    r.launch_from_launcher(0);
    r.launch_from_launcher(1); // opened last: focused
    const uint32_t a = *r.host->window_of("ProbeA"), b = *r.host->window_of("ProbeB");
    ProbeApp& pa = *r.probe("ProbeA");
    ProbeApp& pb = *r.probe("ProbeB");
    auto clear = [&] {
        pa.received.clear();
        pb.received.clear();
    };

    vynex_test::context() = "press and release reach the focused application only";
    clear();
    r.tap('x');
    CHECK(pa.keyboard().empty());
    CHECK(pb.keyboard().size() == 2 && pb.keyboard()[0].code == 'x' && pb.keyboard()[0].pressed && !pb.keyboard()[1].pressed);

    vynex_test::context() = "focus moves with a click";
    r.pointer.click(r.content(a, 50, 50));
    r.settle();
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{a});
    clear();
    r.tap('k');
    CHECK(pb.keyboard().empty());
    CHECK(pa.keyboard().size() == 2 && pa.keyboard()[0].pressed && !pa.keyboard()[1].pressed);

    vynex_test::context() = "a key pressed in one application and released after focus moved";
    clear();
    r.key('y', true);
    r.pointer.click(r.content(b, 50, 50));
    r.settle();
    r.key('y', false);
    CHECK(pa.keyboard().size() == 1 && pa.keyboard()[0].pressed); // its press only: it no longer has focus
    CHECK(pb.keyboard().empty());                                 // never saw the press, so not the release either

    vynex_test::context() = "text goes to the focused application";
    clear();
    r.type("abc");
    CHECK(pa.keyboard().empty());
    CHECK(pb.keyboard().size() == 1 && pb.keyboard()[0].text == "abc");

    vynex_test::context() = "the desktop has the keyboard after a click on it";
    r.pointer.click({1600, 120});
    r.settle();
    CHECK(!r.shell->focused_window().has_value());
    clear();
    r.tap('z');
    r.type("q");
    CHECK(pa.keyboard().empty() && pb.keyboard().empty());

    vynex_test::context() = "the launcher has the keyboard while open; Escape closes it and is not delivered";
    r.pointer.click(r.content(b, 50, 50));
    r.toggle_launcher();
    clear();
    r.tap('m');
    r.type("m");
    r.tap(hal::kKeyEscape);
    r.settle();
    CHECK(!r.launcher_visible());
    CHECK(pa.keyboard().empty() && pb.keyboard().empty());
    clear();
    r.tap('n'); // focus is back with B
    CHECK(pb.keyboard().size() == 2);

    vynex_test::context() = "a minimized application gets nothing";
    r.pointer.click(vynex_test::control_point(*r.wm, b, DecorationHitRegion::MinimizeButton));
    r.settle();
    clear();
    r.tap('p');
    CHECK(pb.keyboard().empty());

    vynex_test::context() = "a closed application gets nothing, and nothing breaks";
    r.wm->set_window_state(b, WindowState::Normal);
    r.shell->focus_window(b);
    r.key('w', true);
    r.close(b);
    r.key('w', false);
    r.tap('e');
    r.type("t");
    CHECK(r.counters_b.alive == 0);
    std::printf("test_keyboard_follows_focus passed\n");
}

void test_terminal_input_and_rendering() {
    Rig r;
    r.launch_from_launcher(2);
    const uint32_t w = *r.host->window_of("Terminal");
    auto* term = dynamic_cast<apps::BasicTerminal*>(r.host->application_in(w));
    CHECK(term != nullptr);
    std::vector<std::string> submitted;
    term->on_input([&](std::string_view line) { submitted.emplace_back(line); });
    CHECK(r.shell->focused_window() == std::optional<uint32_t>{w});
    r.expect_screen_shows(w);

    vynex_test::context() = "typing edits the prompt line, and only that line is redrawn";
    const auto c = *r.wm->get_window_geometry(w);
    const hal::FrameBuffer before = r.display->last;
    r.compositor->invalidated.clear();
    r.type("echo");
    r.type(" hi");
    r.settle();
    CHECK(term->input() == "echo hi");
    CHECK(!r.terminal_toolkit->texts.empty() && r.terminal_toolkit->texts.back().text == "root@vynex:~# echo hi_");
    const int32_t prompt_y = r.terminal_toolkit->texts.back().y;
    const Rect band = apps::BasicTerminal::line_band(prompt_y);
    const Rect prompt_row{c.x, c.y + band.y, c.width, band.height}; // the prompt line's slot, on screen
    for (const Rect& inv : r.compositor->invalidated) {
        if (inv.width == 16 && inv.height == 16) continue; // pointer patches
        CHECK(inv.x >= prompt_row.x && inv.y >= prompt_row.y && inv.x + static_cast<int32_t>(inv.width) <= prompt_row.x + static_cast<int32_t>(prompt_row.width) &&
              inv.y + static_cast<int32_t>(inv.height) <= prompt_row.y + static_cast<int32_t>(prompt_row.height));
    }
    int changed = 0;
    for (size_t i = 0; i < before.pixels.size(); i += 4) {
        if (before.pixels[i] == r.display->last.pixels[i] && before.pixels[i + 1] == r.display->last.pixels[i + 1]) continue;
        const int32_t y = static_cast<int32_t>((i / 4) / before.width);
        CHECK(y >= prompt_row.y && y < prompt_row.y + static_cast<int32_t>(prompt_row.height));
        ++changed;
    }
    CHECK(changed > 0); // the typed text is on screen
    r.expect_screen_shows(w);

    vynex_test::context() = "Backspace and Return";
    r.tap(hal::kKeyBackspace);
    r.tap(hal::kKeyBackspace);
    r.tap(hal::kKeyReturn);
    r.settle();
    CHECK(term->input().empty());
    CHECK(term->history().back() == "root@vynex:~# echo ");
    CHECK(submitted.size() == 1 && submitted[0] == "echo ");
    CHECK(r.terminal_toolkit->texts.back().text == "root@vynex:~# _");
    r.expect_screen_shows(w);

    vynex_test::context() = "output scrolls within a smaller window";
    for (int i = 0; i < 30; ++i) term->write_output("line " + std::to_string(i));
    auto g = *r.wm->get_window_geometry(w);
    g.height = 130; // (130 - 10) / 20 = 6 rows
    r.wm->set_window_geometry(w, g);
    r.settle();
    const auto& texts = r.terminal_toolkit->texts;
    CHECK(texts.size() == 6);
    CHECK(texts.front().text == "line 25" && texts[4].text == "line 29" && texts.back().text == "root@vynex:~# _");
    CHECK(texts.back().y + apps::BasicTerminal::kLineHeight <= static_cast<int32_t>(g.height));
    r.expect_screen_shows(w);

    vynex_test::context() = "closed and reopened: a fresh terminal";
    r.close(w);
    CHECK(!r.host->window_of("Terminal"));
    r.launch_from_launcher(2);
    const uint32_t w2 = *r.host->window_of("Terminal");
    auto* fresh = dynamic_cast<apps::BasicTerminal*>(r.host->application_in(w2));
    CHECK(fresh && fresh->history().size() == 1 && fresh->input().empty());
    r.type("ls");
    r.settle();
    CHECK(fresh->input() == "ls");
    r.expect_screen_shows(w2);
    std::printf("test_terminal_input_and_rendering passed\n");
}

void test_cancelled_press_releases_the_application() {
    Rig r;
    r.launch_from_launcher(0);
    const uint32_t a = *r.host->window_of("ProbeA");
    ProbeApp& pa = *r.probe("ProbeA");
    pa.received.clear();
    r.pointer.send(r.content(a, 30, 40), 1);
    r.pointer.send(r.content(a, 60, 40), 1);
    auto cancel = std::make_shared<core::Event>();
    cancel->topic = hal::kTopicInputPointer;
    cancel->payload = hal::PointerSignalEvent{hal::PointerSignal::Cancel};
    r.bus->dispatch_immediate(cancel);
    vynex_test::context() = "cancelled interaction";
    CHECK(pa.received.size() == 3);
    CHECK(pa.received[2].buttons == 0 && pa.received[2].x == 60 && pa.received[2].y == 40); // the press ended where it was
    // The application no longer holds the pointer: a later press elsewhere is not delivered to it.
    pa.received.clear();
    r.pointer.send({1600, 120}, 0);
    r.pointer.send({1600, 120}, 1);
    r.pointer.send(r.content(a, 10, 10), 1);
    r.pointer.send(r.content(a, 10, 10), 0);
    CHECK(pa.received.empty());
    std::printf("test_cancelled_press_releases_the_application passed\n");
}

} // namespace

int main() {
    std::printf("Running application lifecycle tests...\n");
    test_launcher_opens_applications();
    test_single_instance_close_and_reopen();
    test_keyboard_follows_focus();
    test_terminal_input_and_rendering();
    test_cancelled_press_releases_the_application();
    std::printf("All application lifecycle tests passed.\n");
    return 0;
}
