#include "mock_network_adapter.hpp"

namespace vynexos::hal {

MockNetworkAdapter::MockNetworkAdapter(std::shared_ptr<core::ILogger> logger)
    : m_logger(std::move(logger)) {}

MockNetworkAdapter::~MockNetworkAdapter() {
    shutdown();
}

std::expected<void, NetworkError> MockNetworkAdapter::initialize() {
    if (m_is_initialized) return {};
    m_logger->info("MockNetworkAdapter: Initializing loopback device.");
    m_is_initialized = true;
    return {};
}

void MockNetworkAdapter::set_receive_callback(NetworkReceiveCallback callback) {
    m_receive_callback = std::move(callback);
}

std::expected<void, NetworkError> MockNetworkAdapter::send_packet(std::span<const uint8_t> data) {
    if (!m_is_initialized) return std::unexpected(NetworkError::DeviceNotReady);
    
    m_logger->trace("MockNetworkAdapter: Transmitting packet of {} bytes.", data.size());
    
    // Deterministic loopback: immediately route to receive callback
    if (m_receive_callback) {
        m_logger->trace("MockNetworkAdapter: Routing packet back to receiver.");
        m_receive_callback(data);
    }
    return {};
}

void MockNetworkAdapter::shutdown() {
    if (!m_is_initialized) return;
    m_logger->info("MockNetworkAdapter: Shutting down.");
    m_receive_callback = nullptr;
    m_is_initialized = false;
}

} // namespace vynexos::hal
