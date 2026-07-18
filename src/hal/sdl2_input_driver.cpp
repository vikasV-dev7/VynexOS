#include "sdl2_input_driver.hpp"
#include <SDL.h>

namespace vynexos::hal {

SDL2InputDriver::SDL2InputDriver(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)) 
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
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);
            auto ev = std::make_shared<core::Event>();
            ev->topic = "HAL_INPUT_KEY";
            ev->payload = KeyEvent(static_cast<uint32_t>(event.key.keysym.sym), pressed);
            m_event_bus->publish(ev);
        } else if (event.type == SDL_MOUSEMOTION) {
            uint8_t state = 0;
            if (event.motion.state & SDL_BUTTON_LMASK) state |= 1;
            if (event.motion.state & SDL_BUTTON_RMASK) state |= 2;
            auto ev = std::make_shared<core::Event>();
            ev->topic = "HAL_INPUT_MOUSE";
            ev->payload = MouseEvent(event.motion.x, event.motion.y, state);
            m_event_bus->publish(ev);
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (event.type == SDL_MOUSEBUTTONDOWN) m_mouse_button_state |= 1;
                else m_mouse_button_state &= ~1;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                if (event.type == SDL_MOUSEBUTTONDOWN) m_mouse_button_state |= 2;
                else m_mouse_button_state &= ~2;
            }
            auto ev = std::make_shared<core::Event>();
            ev->topic = "HAL_INPUT_MOUSE";
            ev->payload = MouseEvent(event.button.x, event.button.y, m_mouse_button_state);
            m_event_bus->publish(ev);
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
