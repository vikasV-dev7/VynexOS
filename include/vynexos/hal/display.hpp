#pragma once
#include <expected>
#include <cstdint>
#include <vector>

namespace vynexos::hal {

enum class DisplayError {
    NotAvailable,
    OutOfMemory,
    DriverFailure
};

struct FrameBuffer {
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> pixels;
};

class IDisplay {
public:
    virtual ~IDisplay() = default;
    
    [[nodiscard]] virtual std::expected<void, DisplayError> initialize() = 0;
    [[nodiscard]] virtual std::expected<void, DisplayError> present(const FrameBuffer& buffer) = 0;
    virtual void shutdown() = 0;
};

} // namespace vynexos::hal
