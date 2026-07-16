#pragma once
#include "vynexos/core/ipc_framework.hpp"
#include <unordered_map>
#include <mutex>
#include <string>

namespace vynexos::core {

class LocalIpcEndpoint final : public IIpcEndpoint {
public:
    explicit LocalIpcEndpoint(std::string name);
    ~LocalIpcEndpoint() override = default;

    std::expected<void, IpcError> send(std::span<const uint8_t> payload) override;
    void register_handler(MessageHandler handler) override;
    std::string_view get_endpoint_name() const override;

private:
    std::string m_name;
};

class LocalIpcFramework final : public IIpcFramework {
public:
    std::expected<std::shared_ptr<IIpcEndpoint>, IpcError> create_server(std::string_view name) override;
    std::expected<std::shared_ptr<IIpcEndpoint>, IpcError> connect_client(std::string_view name) override;
    
    // Polls underlying sockets for messages
    void poll_messages();

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<LocalIpcEndpoint>> m_endpoints;
};

} // namespace vynexos::core
