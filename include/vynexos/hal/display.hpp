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

/// Size of the logical desktop coordinate space (independent of the host window size).
struct DisplaySize {
    uint32_t width;
    uint32_t height;
};

/// Default logical desktop size. Display backends are the single source of truth;
/// everything else queries the backend instead of hard-coding dimensions.
inline constexpr DisplaySize kDefaultDisplaySize{1920, 1080};

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
