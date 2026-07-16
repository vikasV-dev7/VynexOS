#pragma once
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>
#include <mutex>

namespace vynexos::desktop {

class MockDisplayBackend final : public IDisplayAbstraction {
public:
    explicit MockDisplayBackend(std::shared_ptr<core::ILogger> logger);
    ~MockDisplayBackend() override;

    std::expected<void, DisplayError> initialize() override;
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override;
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    std::mutex m_mutex;
    bool m_initialized{false};
};

} // namespace vynexos::desktop
