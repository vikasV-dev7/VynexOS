#include "mock_display_backend.hpp"

namespace vynexos::desktop {

MockDisplayBackend::MockDisplayBackend(std::shared_ptr<core::ILogger> logger)
    : m_logger(std::move(logger)) {}

MockDisplayBackend::~MockDisplayBackend() {
    shutdown();
}

std::expected<void, DisplayError> MockDisplayBackend::initialize() {
    std::lock_guard lock(m_mutex);
    if (m_initialized) return {};
    
    m_logger->info("MockDisplayBackend: Initializing Virtual Display (1920x1080)...");
    m_initialized = true;
    return {};
}

std::expected<void, DisplayError> MockDisplayBackend::flush_buffer(const hal::FrameBuffer& buffer) {
    (void)buffer;
    std::lock_guard lock(m_mutex);
    if (!m_initialized) {
        m_logger->error("MockDisplayBackend: Attempted to flush before initialization.");
        return std::unexpected(DisplayError::BackendNotAvailable);
    }
    
    // In a real DRM/KMS backend, we would swap page buffers here.
    m_logger->trace("MockDisplayBackend: Flushed Virtual Framebuffer.");
    return {};
}

void MockDisplayBackend::shutdown() {
    std::lock_guard lock(m_mutex);
    if (m_initialized) {
        m_logger->info("MockDisplayBackend: Shutting down display hardware...");
        m_initialized = false;
    }
}

} // namespace vynexos::desktop
