#include "mock_input_driver.hpp"

namespace vynexos::hal {

MockInputDriver::MockInputDriver(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)) {}

void MockInputDriver::poll() {
    // In a real OS, this would poll /dev/input or PS/2 ports.
    // For the mock, events are injected externally via inject_key / inject_mouse.
}

void MockInputDriver::inject_key(uint32_t key_code, bool pressed) {
    m_logger->trace("MockInputDriver: Hardware IRQ - Key {} {}", key_code, pressed ? "PRESSED" : "RELEASED");
    auto ev = std::make_shared<core::Event>();
    ev->topic = "HAL_INPUT_KEY";
    ev->payload = KeyEvent(key_code, pressed);
    m_event_bus->publish(ev);
}

void MockInputDriver::inject_mouse(int32_t x, int32_t y, uint8_t button_state) {
    m_logger->trace("MockInputDriver: Hardware IRQ - Mouse at {},{} btn: {}", x, y, button_state);
    auto ev = std::make_shared<core::Event>();
    ev->topic = "HAL_INPUT_MOUSE";
    ev->payload = MouseEvent(x, y, button_state);
    m_event_bus->publish(ev);
}

void MockInputDriver::inject_shutdown() {
    m_logger->trace("MockInputDriver: Hardware IRQ - Shutdown");
    m_shutdown_requested = true;
}

bool MockInputDriver::is_shutdown_requested() const {
    return m_shutdown_requested;
}

} // namespace vynexos::hal
