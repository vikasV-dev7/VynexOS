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

    std::shared_ptr<ISurface> create_surface(uint32_t width, uint32_t height) override;
    std::expected<void, DisplayError> render_frame(const SceneGraph& scene) override;

private:
    std::shared_ptr<IDisplayAbstraction> m_display;
    std::shared_ptr<core::ILogger> m_logger;
    std::mutex m_mutex;
    hal::FrameBuffer m_master_fb;
    
    std::expected<void, DisplayError> render_scene_internal(const SceneGraph& scene);
    void blend_surface(hal::FrameBuffer& target, int32_t dst_x, int32_t dst_y, uint32_t target_w, uint32_t target_h,
                       const Rect& clip, float global_opacity, std::shared_ptr<ISurface> surface);
};

} // namespace vynexos::desktop
