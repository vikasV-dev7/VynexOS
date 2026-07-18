#include "basic_compositor.hpp"
#include "software_surface.hpp"
#include <algorithm>
#include <cmath>

namespace vynexos::desktop {

BasicCompositor::BasicCompositor(std::shared_ptr<IDisplayAbstraction> display, std::shared_ptr<core::ILogger> logger)
    : m_display(std::move(display)), m_logger(std::move(logger)) {}

std::shared_ptr<ISurface> BasicCompositor::create_surface(uint32_t width, uint32_t height) {
    return std::make_shared<SoftwareSurface>(width, height);
}

std::expected<void, DisplayError> BasicCompositor::render_frame(const SceneGraph& scene) {
    std::lock_guard lock(m_mutex);
    return render_scene_internal(scene);
}

std::expected<void, DisplayError> BasicCompositor::render_scene_internal(const SceneGraph& scene) {
    if (m_master_fb.pixels.empty()) {
        m_master_fb.width = 1920;
        m_master_fb.height = 1080;
        m_master_fb.pixels.resize(m_master_fb.width * m_master_fb.height * 4, 0); // Clear to black
    } else {
        std::fill(m_master_fb.pixels.begin(), m_master_fb.pixels.end(), 0);
    }
    
    for (const auto& layer : scene) {
        if (!layer.visible || layer.opacity <= 0.0f) continue;
        
        Rect clip = layer.clip;
        if (clip.width == 0 || clip.height == 0) {
            clip = {0, 0, m_master_fb.width, m_master_fb.height};
        }
        
        blend_surface(m_master_fb, layer.bounds.x, layer.bounds.y, m_master_fb.width, m_master_fb.height, clip, layer.opacity, layer.surface);
    }
    
    return m_display->flush_buffer(m_master_fb);
}

void BasicCompositor::blend_surface(hal::FrameBuffer& target, int32_t dst_x, int32_t dst_y, uint32_t target_w, uint32_t target_h,
                                    const Rect& clip, float global_opacity, std::shared_ptr<ISurface> surface) {
    if (!surface) return;
    auto mapped = surface->map_pixels();
    if (mapped.empty()) return;

    uint32_t surf_w = surface->width();
    uint32_t surf_h = surface->height();

    int32_t start_x = std::max(dst_x, clip.x);
    int32_t start_y = std::max(dst_y, clip.y);
    int32_t end_x = std::min(dst_x + static_cast<int32_t>(surf_w), std::min(clip.x + static_cast<int32_t>(clip.width), static_cast<int32_t>(target_w)));
    int32_t end_y = std::min(dst_y + static_cast<int32_t>(surf_h), std::min(clip.y + static_cast<int32_t>(clip.height), static_cast<int32_t>(target_h)));

    for (int32_t y = start_y; y < end_y; ++y) {
        for (int32_t x = start_x; x < end_x; ++x) {
            int32_t src_x = x - dst_x;
            int32_t src_y = y - dst_y;
            
            size_t src_idx = (static_cast<size_t>(src_y) * static_cast<size_t>(surf_w) + static_cast<size_t>(src_x)) * 4;
            size_t dst_idx = (static_cast<size_t>(y) * static_cast<size_t>(target_w) + static_cast<size_t>(x)) * 4;

            if (src_idx + 3 < mapped.size() && dst_idx + 3 < target.pixels.size()) {
                uint8_t sr = mapped[src_idx];
                uint8_t sg = mapped[src_idx + 1];
                uint8_t sb = mapped[src_idx + 2];
                uint8_t sa = static_cast<uint8_t>(mapped[src_idx + 3] * global_opacity);

                if (sa == 255) {
                    target.pixels[dst_idx] = sr;
                    target.pixels[dst_idx + 1] = sg;
                    target.pixels[dst_idx + 2] = sb;
                    target.pixels[dst_idx + 3] = 255;
                } else if (sa > 0) {
                    float alpha = sa / 255.0f;
                    float inv_alpha = 1.0f - alpha;
                    
                    uint8_t dr = target.pixels[dst_idx];
                    uint8_t dg = target.pixels[dst_idx + 1];
                    uint8_t db = target.pixels[dst_idx + 2];

                    target.pixels[dst_idx] = static_cast<uint8_t>(sr + (dr * inv_alpha));
                    target.pixels[dst_idx + 1] = static_cast<uint8_t>(sg + (dg * inv_alpha));
                    target.pixels[dst_idx + 2] = static_cast<uint8_t>(sb + (db * inv_alpha));
                    target.pixels[dst_idx + 3] = std::max(target.pixels[dst_idx + 3], sa);
                }
            }
        }
    }
}

} // namespace vynexos::desktop
