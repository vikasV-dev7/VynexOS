#pragma once
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/hal/viewport.hpp"
#include <memory>
#include <mutex>

namespace vynexos::desktop {

class SDL2DisplayBackend final : public IDisplayAbstraction, public hal::IPointerMapper {
public:
    explicit SDL2DisplayBackend(std::shared_ptr<core::ILogger> logger);
    ~SDL2DisplayBackend() override;

    std::expected<void, DisplayError> initialize() override;
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override;
    void shutdown() override;
    hal::DisplaySize logical_size() const override { return m_logical_size; }
    void refresh() override;

    /// Maps SDL window coordinates (window units) to logical desktop coordinates, accounting for
    /// letterboxing and the window-unit to pixel ratio (high-DPI).
    hal::LogicalPoint to_logical(int32_t window_x, int32_t window_y) const override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    mutable std::mutex m_mutex;
    bool m_initialized{false};
    hal::DisplaySize m_logical_size{hal::kDefaultDisplaySize};

    struct SDLContext;
    std::unique_ptr<SDLContext> m_ctx;

    void present_locked();
};

} // namespace vynexos::desktop
