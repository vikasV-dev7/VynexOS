#include "local_ipc_framework.hpp"

namespace vynexos::core {

LocalIpcEndpoint::LocalIpcEndpoint(std::string name) : m_name(std::move(name)) {}

std::expected<void, IpcError> LocalIpcEndpoint::send(std::span<const uint8_t> payload) {
    (void)payload;
    // Simulated transmission
    return {};
}

void LocalIpcEndpoint::register_handler(MessageHandler handler) {
    (void)handler;
    // Stub
}

std::string_view LocalIpcEndpoint::get_endpoint_name() const {
    return m_name;
}


std::expected<std::shared_ptr<IIpcEndpoint>, IpcError> LocalIpcFramework::create_server(std::string_view name) {
    std::lock_guard lock(m_mutex);
    std::string key(name);
    if (m_endpoints.contains(key)) {
        return std::unexpected(IpcError::EndpointExists);
    }
    
    auto endpoint = std::make_shared<LocalIpcEndpoint>(key);
    m_endpoints[key] = endpoint;
    return endpoint;
}

std::expected<std::shared_ptr<IIpcEndpoint>, IpcError> LocalIpcFramework::connect_client(std::string_view name) {
    std::lock_guard lock(m_mutex);
    std::string key(name);
    auto it = m_endpoints.find(key);
    if (it != m_endpoints.end()) {
        return it->second;
    }
    return std::unexpected(IpcError::ConnectionRefused);
}

void LocalIpcFramework::poll_messages() {
    // In a full POSIX implementation, this would call epoll() or select() 
    // across all connected file descriptors to flush incoming buffers to the Event Bus.
    // For now, it's a structural placeholder for the event loop.
}

} // namespace vynexos::core
