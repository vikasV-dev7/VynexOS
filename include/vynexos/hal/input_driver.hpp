#pragma once
#include <cstdint>
#include <memory>
#include "vynexos/core/event_bus.hpp"

namespace vynexos::hal {

struct KeyEvent {
    uint32_t key_code;
    bool pressed;
    KeyEvent(uint32_t k, bool p) : key_code(k), pressed(p) {}
};

struct MouseEvent {
    int32_t x;
    int32_t y;
    uint8_t button_state;
    MouseEvent(int32_t _x, int32_t _y, uint8_t _b) : x(_x), y(_y), button_state(_b) {}
};

class IInputDriver {
public:
    virtual ~IInputDriver() = default;
    virtual void poll() = 0;
    virtual bool is_shutdown_requested() const = 0;
};

} // namespace vynexos::hal
