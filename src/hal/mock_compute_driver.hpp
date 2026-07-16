#pragma once
#include "vynexos/hal/compute.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>
#include <mutex>

namespace vynexos::hal {

class MockComputeDriver final : public IComputeDevice {
public:
    explicit MockComputeDriver(std::shared_ptr<core::ILogger> logger);
    ~MockComputeDriver() override = default;

    [[nodiscard]] std::expected<void, ComputeError> initialize() override;
    [[nodiscard]] std::expected<void, ComputeError> execute_kernel(std::span<const uint8_t> shader_code) override;
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    mutable std::mutex m_mutex;
    bool m_initialized{false};
};

} // namespace vynexos::hal
