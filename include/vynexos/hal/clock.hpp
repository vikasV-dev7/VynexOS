#pragma once
#include <expected>
#include <cstdint>

namespace vynexos::hal {

enum class ClockError {
    DeviceNotReady,
    DriverFailure
};

class IHardwareClock {
public:
    virtual ~IHardwareClock() = default;
    
    [[nodiscard]] virtual std::expected<void, ClockError> initialize() = 0;
    
    // Returns monotonic time in nanoseconds since the device started.
    [[nodiscard]] virtual uint64_t get_monotonic_time_ns() const = 0;
    
    virtual void shutdown() = 0;
};

} // namespace vynexos::hal
