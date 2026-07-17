#pragma once
#include <vynexos/hal/clock.hpp>
#include <vynexos/core/logger.hpp>
#include <memory>
#include <chrono>

namespace vynexos::hal {

class MockHardwareClock : public IHardwareClock {
public:
    explicit MockHardwareClock(std::shared_ptr<core::ILogger> logger);
    ~MockHardwareClock() override;

    [[nodiscard]] std::expected<void, ClockError> initialize() override;
    
    [[nodiscard]] uint64_t get_monotonic_time_ns() const override;
    
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    std::chrono::steady_clock::time_point m_start_time;
    bool m_is_initialized = false;
};

} // namespace vynexos::hal
