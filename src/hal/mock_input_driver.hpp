#pragma once
#include "vynexos/hal/input_driver.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>

namespace vynexos::hal {

class MockInputDriver final : public IInputDriver {
public:
    MockInputDriver(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger);
    ~MockInputDriver() override = default;

    void poll() override;
    
    // Test API to manually trigger synthetic hardware interrupts
    void inject_key(uint32_t key_code, bool pressed);
    void inject_mouse(int32_t x, int32_t y, uint8_t button_state);

private:
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::ILogger> m_logger;
};

} // namespace vynexos::hal
