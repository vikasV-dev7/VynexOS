#pragma once
#include <expected>
#include <cstdint>
#include <span>

namespace vynexos::hal {

enum class AudioError {
    DeviceNotFound,
    BufferUnderrun,
    DriverFailure
};

class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;
    
    [[nodiscard]] virtual std::expected<void, AudioError> initialize() = 0;
    [[nodiscard]] virtual std::expected<void, AudioError> play(std::span<const int16_t> pcm_data) = 0;
    virtual void shutdown() = 0;
};

} // namespace vynexos::hal
