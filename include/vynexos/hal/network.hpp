#pragma once
#include <expected>
#include <cstdint>
#include <span>
#include <functional>

namespace vynexos::hal {

enum class NetworkError {
    DeviceNotReady,
    TransmissionFailed,
    DriverFailure
};

using NetworkReceiveCallback = std::function<void(std::span<const uint8_t>)>;

class INetworkAdapter {
public:
    virtual ~INetworkAdapter() = default;
    
    [[nodiscard]] virtual std::expected<void, NetworkError> initialize() = 0;
    
    // Register a callback to be invoked when a packet is received.
    virtual void set_receive_callback(NetworkReceiveCallback callback) = 0;
    
    // Transmits a raw network packet asynchronously.
    [[nodiscard]] virtual std::expected<void, NetworkError> send_packet(std::span<const uint8_t> data) = 0;
    
    virtual void shutdown() = 0;
};

} // namespace vynexos::hal
