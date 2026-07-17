#include "mock_hardware_clock.hpp"

namespace vynexos::hal {

MockHardwareClock::MockHardwareClock(std::shared_ptr<core::ILogger> logger)
    : m_logger(std::move(logger)) {}

MockHardwareClock::~MockHardwareClock() {
    shutdown();
}

std::expected<void, ClockError> MockHardwareClock::initialize() {
    if (m_is_initialized) return {};
    m_logger->info("MockHardwareClock: Initializing high-resolution timer.");
    m_start_time = std::chrono::steady_clock::now();
    m_is_initialized = true;
    return {};
}

uint64_t MockHardwareClock::get_monotonic_time_ns() const {
    if (!m_is_initialized) return 0;
    
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_start_time).count());
}

void MockHardwareClock::shutdown() {
    if (!m_is_initialized) return;
    m_logger->info("MockHardwareClock: Shutting down.");
    m_is_initialized = false;
}

} // namespace vynexos::hal
