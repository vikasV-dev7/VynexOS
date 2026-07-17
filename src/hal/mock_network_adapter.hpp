#pragma once
#include <vynexos/hal/network.hpp>
#include <vynexos/core/logger.hpp>
#include <memory>
#include <vector>

namespace vynexos::hal {

class MockNetworkAdapter : public INetworkAdapter {
public:
    explicit MockNetworkAdapter(std::shared_ptr<core::ILogger> logger);
    ~MockNetworkAdapter() override;

    [[nodiscard]] std::expected<void, NetworkError> initialize() override;
    void set_receive_callback(NetworkReceiveCallback callback) override;
    [[nodiscard]] std::expected<void, NetworkError> send_packet(std::span<const uint8_t> data) override;
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    NetworkReceiveCallback m_receive_callback;
    bool m_is_initialized = false;
};

} // namespace vynexos::hal
