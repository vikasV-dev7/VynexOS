#pragma once
#include "vynexos/hal/input_driver.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>

namespace vynexos::hal {

class SDL2InputDriver final : public IInputDriver {
public:
    SDL2InputDriver(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger);
    ~SDL2InputDriver() override;

    void poll() override;
    bool is_shutdown_requested() const override;

private:
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::ILogger> m_logger;
    bool m_shutdown_requested{false};
    uint8_t m_mouse_button_state{0};
};

} // namespace vynexos::hal
