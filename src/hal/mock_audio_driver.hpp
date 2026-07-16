#pragma once
#include "vynexos/hal/audio.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>
#include <mutex>

namespace vynexos::hal {

class MockAudioDriver final : public IAudioDevice {
public:
    explicit MockAudioDriver(std::shared_ptr<core::ILogger> logger);
    ~MockAudioDriver() override = default;

    [[nodiscard]] std::expected<void, AudioError> initialize() override;
    [[nodiscard]] std::expected<void, AudioError> play(std::span<const int16_t> pcm_data) override;
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    mutable std::mutex m_mutex;
    bool m_initialized{false};
};

} // namespace vynexos::hal
