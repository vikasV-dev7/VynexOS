#pragma once
#include <expected>
#include <cstdint>
#include <span>

namespace vynexos::hal {

enum class ComputeError {
    DeviceNotFound,
    OutOfMemory,
    ExecutionFailed
};

class IComputeDevice {
public:
    virtual ~IComputeDevice() = default;
    
    [[nodiscard]] virtual std::expected<void, ComputeError> initialize() = 0;
    [[nodiscard]] virtual std::expected<void, ComputeError> execute_kernel(std::span<const uint8_t> shader_code) = 0;
    virtual void shutdown() = 0;
};

} // namespace vynexos::hal
