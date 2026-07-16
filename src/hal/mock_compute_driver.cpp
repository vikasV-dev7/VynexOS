#include "mock_compute_driver.hpp"

namespace vynexos::hal {

MockComputeDriver::MockComputeDriver(std::shared_ptr<core::ILogger> logger)
    : m_logger(std::move(logger)) {}

std::expected<void, ComputeError> MockComputeDriver::initialize() {
    std::lock_guard lock(m_mutex);
    if (m_initialized) {
        return {};
    }
    
    m_logger->info("MockComputeDriver: Initialized mock compute backend.");
    m_initialized = true;
    return {};
}

std::expected<void, ComputeError> MockComputeDriver::execute_kernel(std::span<const uint8_t> shader_code) {
    std::lock_guard lock(m_mutex);
    if (!m_initialized) {
        m_logger->error("MockComputeDriver: Execution called before initialization.");
        return std::unexpected(ComputeError::DeviceNotFound);
    }
    
    // In a VM mock environment, we just log the compute kernel simulation
    m_logger->trace("MockComputeDriver: Simulated execution of {} bytes of shader code", shader_code.size_bytes());
    return {};
}

void MockComputeDriver::shutdown() {
    std::lock_guard lock(m_mutex);
    if (m_initialized) {
        m_logger->info("MockComputeDriver: Shutting down mock compute backend.");
        m_initialized = false;
    }
}

} // namespace vynexos::hal
