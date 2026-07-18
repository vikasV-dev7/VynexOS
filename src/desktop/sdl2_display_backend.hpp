#pragma once
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>
#include <mutex>

namespace vynexos::desktop {

class SDL2DisplayBackend final : public IDisplayAbstraction {
public:
    explicit SDL2DisplayBackend(std::shared_ptr<core::ILogger> logger);
    ~SDL2DisplayBackend() override;

    std::expected<void, DisplayError> initialize() override;
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override;
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    std::mutex m_mutex;
    bool m_initialized{false};
    
    struct SDLContext;
    std::unique_ptr<SDLContext> m_ctx;
};

} // namespace vynexos::desktop
