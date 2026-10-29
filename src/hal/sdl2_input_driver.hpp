#pragma once
#include "vynexos/hal/input_driver.hpp"
#include "vynexos/hal/viewport.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>

namespace vynexos::hal {

class SDL2InputDriver final : public IInputDriver {
public:
    /// @param pointer_mapper Converts SDL window coordinates to logical desktop coordinates.
    ///        When null, window coordinates are published unchanged.
    SDL2InputDriver(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger,
                    std::shared_ptr<const IPointerMapper> pointer_mapper = nullptr);
    ~SDL2InputDriver() override;

    void poll() override;
    bool is_shutdown_requested() const override;

private:
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::ILogger> m_logger;
    std::shared_ptr<const IPointerMapper> m_pointer_mapper;
    bool m_shutdown_requested{false};
    uint8_t m_mouse_button_state{0};
};

} // namespace vynexos::hal
