// The whole keyboard path, from SDL to the focused application: real SDL events (SDL_PushEvent), the real SDL2 input
// driver (poll), the event bus, the desktop shell's routing, and the Terminal hosted through the application host. The
// test checks what the application actually received and shows, not that an SDL event was produced.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "../apps/application_host.hpp"
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
#include "../hal/sdl2_input_driver.hpp"
#include "vynexos/apps/application_registry.hpp"
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

class NullDisplay final : public IDisplayAbstraction {
public:
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer&) override { return {}; }
    void shutdown() override {}
};

void push_key(SDL_Keycode key, bool down) {
    SDL_Event ev{};
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = key;
    CHECK(SDL_PushEvent(&ev) == 1);
}
void push_text(const char* text) {
    SDL_Event ev{};
    ev.type = SDL_TEXTINPUT;
    std::memcpy(ev.text.text, text, std::min(std::strlen(text), sizeof(ev.text.text) - 1)); // zero-filled: stays terminated
    CHECK(SDL_PushEvent(&ev) == 1);
}

void test_sdl_keyboard_reaches_the_focused_terminal() {
    auto logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    auto bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    hal::SDL2InputDriver driver(bus, logger); // initializes SDL's event subsystem
    auto compositor = std::make_shared<BasicCompositor>(std::make_shared<NullDisplay>(), logger);
    auto wm = std::make_shared<BasicWindowManager>(bus, logger);
    auto toolkit = std::make_shared<BasicWidgetToolkit>();
    auto shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, std::make_shared<BasicWindowDecorator>(toolkit),
                                                      std::make_shared<BasicTaskbar>(toolkit), bus);
    auto registry = std::make_shared<apps::ApplicationRegistry>();
    auto host = std::make_shared<apps::ApplicationHost>(registry, wm, compositor, shell);
    shell->set_application_host(host.get());
    host->register_application({"Terminal", "Terminal", {100, 100, 600, 400}}, [toolkit]() { return std::make_unique<apps::BasicTerminal>(toolkit); });
    host->register_application({"DemoApp", "Demo App", {800, 300, 300, 200}}, [toolkit]() { return std::make_unique<apps::DemoApp>(toolkit); });
    shell->launch();
    auto frame = [&] {
        shell->update_frame();
        CHECK(compositor->render_frame(wm->build_scene()).has_value());
    };

    const uint32_t terminal_window = *host->open("Terminal");
    auto* terminal = dynamic_cast<apps::BasicTerminal*>(host->application_in(terminal_window));
    CHECK(terminal != nullptr);
    frame();

    vynex_test::context() = "typing into the focused Terminal through SDL";
    push_key(SDLK_h, true); // key events for printable keys carry no text: the text arrives as SDL_TEXTINPUT
    push_text("h");
    push_key(SDLK_h, false);
    push_text("i!");
    push_key(SDLK_BACKSPACE, true);
    push_key(SDLK_BACKSPACE, false);
    driver.poll();
    frame();
    CHECK(terminal->input() == "hi");
    push_key(SDLK_RETURN, true);
    push_key(SDLK_RETURN, false);
    driver.poll();
    frame();
    CHECK(terminal->input().empty());
    CHECK(terminal->history().back() == std::string(apps::BasicTerminal::kPrompt) + "hi");
    push_text("kp");
    push_key(SDLK_KP_ENTER, true); // the keypad Enter submits too
    push_key(SDLK_KP_ENTER, false);
    driver.poll();
    frame();
    CHECK(terminal->input().empty());
    CHECK(terminal->history().back() == std::string(apps::BasicTerminal::kPrompt) + "kp");
    CHECK(static_cast<uint32_t>(SDLK_KP_ENTER) == hal::kKeyKeypadEnter && static_cast<uint32_t>(SDLK_RETURN) == hal::kKeyReturn &&
          static_cast<uint32_t>(SDLK_BACKSPACE) == hal::kKeyBackspace && static_cast<uint32_t>(SDLK_ESCAPE) == hal::kKeyEscape);

    vynex_test::context() = "another window has the focus: the Terminal gets nothing";
    const uint32_t demo_window = *host->open("DemoApp");
    CHECK(shell->focused_window() == std::optional<uint32_t>{demo_window});
    push_text("zz");
    push_key(SDLK_RETURN, true);
    push_key(SDLK_RETURN, false);
    driver.poll();
    frame();
    CHECK(terminal->input().empty());
    CHECK(terminal->history().back() == std::string(apps::BasicTerminal::kPrompt) + "kp");

    vynex_test::context() = "focus back on the Terminal";
    shell->focus_window(terminal_window);
    push_text("ok");
    driver.poll();
    CHECK(terminal->input() == "ok");

    vynex_test::context() = "closed: input goes nowhere";
    CHECK(host->close(terminal_window));
    push_text("lost");
    push_key(SDLK_RETURN, true);
    push_key(SDLK_RETURN, false);
    driver.poll();
    frame();
    CHECK(registry->get_running_applications().size() == 1); // only the Demo App

    host->close_all();
    shell->set_application_host(nullptr);
    std::printf("test_sdl_keyboard_reaches_the_focused_terminal passed\n");
}

} // namespace

int main(int, char**) {
    std::printf("Running SDL keyboard path tests...\n");
    test_sdl_keyboard_reaches_the_focused_terminal();
    std::printf("All SDL keyboard path tests passed.\n");
    return 0;
}
