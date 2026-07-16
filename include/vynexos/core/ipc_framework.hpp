#pragma once
#include <vector>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <memory>
#include <string_view>

namespace vynexos::core {

enum class IpcError {
    ConnectionFailed,
    Timeout,
    MessageTooLarge,
    Disconnected,
    EndpointExists,
    ConnectionRefused
};

class IIpcEndpoint {
public:
    virtual ~IIpcEndpoint() = default;
    
    using MessageHandler = std::function<void(std::span<const uint8_t>)>;
    
    virtual std::expected<void, IpcError> send(std::span<const uint8_t> data) = 0;
    virtual void register_handler(MessageHandler handler) = 0;
    virtual std::string_view get_endpoint_name() const = 0;
};

class IIpcFramework {
public:
    virtual ~IIpcFramework() = default;
    
    // Ownership Model Justification:
    // LocalIpcFramework manages connections via an internal std::unordered_map.
    // It must return a shared_ptr to the caller (client/server) so that both the
    // framework (for routing incoming messages) and the caller (for sending messages)
    // share overlapping lifespans. Returning unique_ptr would prevent the framework
    // from safely retaining a handle to the endpoint if the caller disconnects.
    virtual std::expected<std::shared_ptr<IIpcEndpoint>, IpcError> create_server(std::string_view name) = 0;
    virtual std::expected<std::shared_ptr<IIpcEndpoint>, IpcError> connect_client(std::string_view name) = 0;
};

} // namespace vynexos::core
