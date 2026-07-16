#pragma once
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/core/logger.hpp"
#include <mutex>
#include <unordered_map>
#include <memory>

namespace vynexos::desktop {

class BasicCompositor final : public ICompositor {
public:
    BasicCompositor(std::shared_ptr<IDisplayAbstraction> display, std::shared_ptr<core::ILogger> logger);
    ~BasicCompositor() override = default;

    void submit_buffer(const WindowBuffer& buffer) override;
    std::expected<void, DisplayError> render_frame() override;

private:
    std::shared_ptr<IDisplayAbstraction> m_display;
    std::shared_ptr<core::ILogger> m_logger;
    std::mutex m_mutex;
    
    std::unordered_map<uint32_t, WindowBuffer> m_window_buffers;
};

} // namespace vynexos::desktop
