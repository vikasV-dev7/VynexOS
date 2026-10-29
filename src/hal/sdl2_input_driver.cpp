#include "sdl2_input_driver.hpp"
#include <SDL.h>

namespace vynexos::hal {

SDL2InputDriver::SDL2InputDriver(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger,
                                 std::shared_ptr<const IPointerMapper> pointer_mapper)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)), m_pointer_mapper(std::move(pointer_mapper))
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
        m_logger->error("SDL_InitSubSystem(EVENTS) failed");
    } else {
        m_logger->info("SDL2 Input Driver initialized");
    }
}

SDL2InputDriver::~SDL2InputDriver() {
    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }
}

void SDL2InputDriver::poll() {
    // Input is dispatched synchronously on the polling (UI) thread so that consumers observe
    // events in exactly the order the OS produced them and never run concurrently with the frame loop.
    auto to_logical = [this](int32_t x, int32_t y) {
        return m_pointer_mapper ? m_pointer_mapper->to_logical(x, y) : LogicalPoint{x, y};
    };

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);
            auto ev = std::make_shared<core::Event>();
            ev->topic = kTopicInputKey;
            ev->payload = KeyEvent(static_cast<uint32_t>(event.key.keysym.sym), pressed);
            m_event_bus->dispatch_immediate(ev);
        } else if (event.type == SDL_MOUSEMOTION) {
            uint8_t state = 0;
            if (event.motion.state & SDL_BUTTON_LMASK) state |= 1;
            if (event.motion.state & SDL_BUTTON_RMASK) state |= 2;
            auto ev = std::make_shared<core::Event>();
            const LogicalPoint pt = to_logical(event.motion.x, event.motion.y);
            ev->topic = kTopicInputMouse;
            ev->payload = MouseEvent(pt.x, pt.y, state);
            m_event_bus->dispatch_immediate(ev);
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (event.type == SDL_MOUSEBUTTONDOWN) m_mouse_button_state |= 1;
                else m_mouse_button_state &= static_cast<uint8_t>(~1);
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (event.type == SDL_MOUSEBUTTONDOWN) m_mouse_button_state |= 2;
                else m_mouse_button_state &= static_cast<uint8_t>(~2);
            }
            auto ev = std::make_shared<core::Event>();
            const LogicalPoint pt = to_logical(event.button.x, event.button.y);
            ev->topic = kTopicInputMouse;
            ev->payload = MouseEvent(pt.x, pt.y, m_mouse_button_state);
            m_event_bus->dispatch_immediate(ev);
        } else if (event.type == SDL_QUIT) {
            m_logger->info("SDL_QUIT received (Window closed)");
            m_shutdown_requested = true;
        }
    }
}

bool SDL2InputDriver::is_shutdown_requested() const {
    return m_shutdown_requested;
}

} // namespace vynexos::hal
