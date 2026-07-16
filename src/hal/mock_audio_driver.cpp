#include "mock_audio_driver.hpp"

namespace vynexos::hal {

MockAudioDriver::MockAudioDriver(std::shared_ptr<core::ILogger> logger)
    : m_logger(std::move(logger)) {}

std::expected<void, AudioError> MockAudioDriver::initialize() {
    std::lock_guard lock(m_mutex);
    if (m_initialized) {
        return {};
    }
    
    m_logger->info("MockAudioDriver: Initialized mock audio backend.");
    m_initialized = true;
    return {};
}

std::expected<void, AudioError> MockAudioDriver::play(std::span<const int16_t> pcm_data) {
    std::lock_guard lock(m_mutex);
    if (!m_initialized) {
        m_logger->error("MockAudioDriver: Play called before initialization.");
        return std::unexpected(AudioError::DeviceNotFound);
    }
    
    // In a VM mock environment, we just log that we would have played audio
    m_logger->trace("MockAudioDriver: Simulated playing {} bytes of PCM data", pcm_data.size_bytes());
    return {};
}

void MockAudioDriver::shutdown() {
    std::lock_guard lock(m_mutex);
    if (m_initialized) {
        m_logger->info("MockAudioDriver: Shutting down mock audio backend.");
        m_initialized = false;
    }
}

} // namespace vynexos::hal
