#include "basic_compositor.hpp"
#include "software_surface.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

namespace vynexos::desktop {

namespace {

// A half-open span [begin, end) in 64-bit arithmetic: `x + width` of a Rect can overflow int32_t, so every bound is
// computed here before anything is narrowed back.
struct Span64 {
    int64_t begin;
    int64_t end;
};

Span64 x_span(const Rect& r) { return {r.x, static_cast<int64_t>(r.x) + r.width}; }
Span64 y_span(const Rect& r) { return {r.y, static_cast<int64_t>(r.y) + r.height}; }

// The part of `r` inside a `width` x `height` framebuffer, or nothing if it lies entirely outside (or is empty).
// Windows may be anywhere, including partly or wholly off-screen at negative coordinates; the result never is.
std::optional<Rect> clamp_to_framebuffer(const Rect& r, uint32_t width, uint32_t height) {
    const Span64 xs = x_span(r);
    const Span64 ys = y_span(r);
    const int64_t x0 = std::max<int64_t>(xs.begin, 0);
    const int64_t y0 = std::max<int64_t>(ys.begin, 0);
    const int64_t x1 = std::min<int64_t>(xs.end, width);
    const int64_t y1 = std::min<int64_t>(ys.end, height);
    if (x0 >= x1 || y0 >= y1) return std::nullopt;
    return Rect{static_cast<int32_t>(x0), static_cast<int32_t>(y0), static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)};
}

// Intersection of two rectangles that are already inside the framebuffer (so their bounds fit in int32_t).
std::optional<Rect> intersect_on_screen(const Rect& a, const Rect& b) {
    const int64_t x0 = std::max(x_span(a).begin, x_span(b).begin);
    const int64_t y0 = std::max(y_span(a).begin, y_span(b).begin);
    const int64_t x1 = std::min(x_span(a).end, x_span(b).end);
    const int64_t y1 = std::min(y_span(a).end, y_span(b).end);
    if (x0 >= x1 || y0 >= y1) return std::nullopt;
    return Rect{static_cast<int32_t>(x0), static_cast<int32_t>(y0), static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)};
}

} // namespace

BasicCompositor::BasicCompositor(std::shared_ptr<IDisplayAbstraction> display, std::shared_ptr<core::ILogger> logger)
    : m_display(std::move(display)), m_logger(std::move(logger)) {}

hal::DisplaySize BasicCompositor::logical_size() const {
    return m_display->logical_size();
}

std::shared_ptr<ISurface> BasicCompositor::create_surface(uint32_t width, uint32_t height) {
    return std::make_shared<SoftwareSurface>(width, height);
}

void BasicCompositor::invalidate_region(const Rect& rect) {
    // Off-screen parts of a region (e.g. a window dragged past the edge) have nothing to redraw. Clamping here also keeps
    // the dirty-region set's own int32 arithmetic within range.
    const auto size = m_display->logical_size();
    const auto on_screen = clamp_to_framebuffer(rect, size.width, size.height);
    if (!on_screen) return;
    std::lock_guard lock(m_mutex);
    m_dirty_regions.add_region(*on_screen);
}

std::expected<void, DisplayError> BasicCompositor::render_frame(const SceneGraph& scene) {
    std::lock_guard lock(m_mutex);
    return render_scene_internal(scene);
}

std::expected<void, DisplayError> BasicCompositor::render_scene_internal(const SceneGraph& scene) {
    if (m_master_fb.pixels.empty()) {
        const auto size = m_display->logical_size();
        m_master_fb.width = size.width;
        m_master_fb.height = size.height;
        m_master_fb.pixels.resize(m_master_fb.width * m_master_fb.height * 4, static_cast<uint8_t>(0));
    }
    
    if (m_first_frame) {
        m_dirty_regions.add_region({0, 0, m_master_fb.width, m_master_fb.height});
        m_first_frame = false;
    }
    
    if (m_dirty_regions.is_empty()) {
        // Nothing to redraw; let the backend re-present if the host window changed.
        m_display->refresh();
        return {};
    }
    
    auto regions = m_dirty_regions.get_regions();
    m_dirty_regions.clear();
    
    const uint32_t fb_w = m_master_fb.width;
    const uint32_t fb_h = m_master_fb.height;
    for (const auto& region : regions) {
        // Every rectangle used below is first clamped to the framebuffer, so nothing derived from it can index outside.
        const auto dirty = clamp_to_framebuffer(region, fb_w, fb_h);
        if (!dirty) continue;

        uint8_t* ptr = m_master_fb.pixels.data();
        for (uint32_t y = 0; y < dirty->height; ++y) {
            const size_t row_idx = (static_cast<size_t>(dirty->y + static_cast<int32_t>(y)) * fb_w + static_cast<size_t>(dirty->x)) * 4;
            std::memset(&ptr[row_idx], 0, static_cast<size_t>(dirty->width) * 4);
        }

        for (const auto& layer : scene) {
            if (!layer.visible || layer.opacity <= 0.0f) continue;

            // A zero-sized clip means "unclipped". An explicit clip (e.g. a window's visible frame) may extend off-screen.
            const bool unclipped = layer.clip.width == 0 || layer.clip.height == 0;
            const auto clip = unclipped ? std::optional<Rect>{Rect{0, 0, fb_w, fb_h}} : clamp_to_framebuffer(layer.clip, fb_w, fb_h);
            if (!clip) continue;

            const auto on_screen = clamp_to_framebuffer(layer.bounds, fb_w, fb_h);
            if (!on_screen) continue; // the layer is entirely off-screen

            const auto in_dirty = intersect_on_screen(*dirty, *on_screen);
            if (!in_dirty) continue;
            const auto final_clip = intersect_on_screen(*clip, *in_dirty);
            if (!final_clip) continue;

            blend_surface(m_master_fb, layer.bounds.x, layer.bounds.y, fb_w, fb_h, *final_clip, layer.opacity, layer.surface);
        }
    }
    
    return m_display->flush_buffer(m_master_fb);
}

void BasicCompositor::blend_surface(hal::FrameBuffer& target, int32_t dst_x, int32_t dst_y, uint32_t target_w, uint32_t target_h,
                                    const Rect& clip, float global_opacity, std::shared_ptr<ISurface> surface) {
    if (!surface) return;
    auto mapped = surface->map_pixels();
    if (mapped.empty()) return;

    const uint32_t surf_w = surface->width();
    const uint32_t surf_h = surface->height();
    // Never trust declared sizes beyond the memory actually there.
    if (mapped.size() < static_cast<size_t>(surf_w) * surf_h * 4) return;
    if (target.pixels.size() < static_cast<size_t>(target_w) * target_h * 4) return;

    // Destination pixels to write: inside the surface's placement, the clip and the target, in 64-bit arithmetic. With
    // start >= max(dst, 0) and end <= min(dst + surf, target), every source index is inside the surface and every destination
    // index inside the target, whatever the inputs (negative placement, clips reaching off-screen, empty rectangles).
    const int64_t start_x = std::max({static_cast<int64_t>(dst_x), x_span(clip).begin, int64_t{0}});
    const int64_t start_y = std::max({static_cast<int64_t>(dst_y), y_span(clip).begin, int64_t{0}});
    const int64_t end_x = std::min({static_cast<int64_t>(dst_x) + surf_w, x_span(clip).end, static_cast<int64_t>(target_w)});
    const int64_t end_y = std::min({static_cast<int64_t>(dst_y) + surf_h, y_span(clip).end, static_cast<int64_t>(target_h)});
    if (start_x >= end_x || start_y >= end_y) return;

    uint8_t* dst_ptr = target.pixels.data();
    const uint8_t* src_ptr = mapped.data();

    for (int64_t y = start_y; y < end_y; ++y) {
        const int64_t src_y = y - dst_y;
        size_t dst_idx = (static_cast<size_t>(y) * target_w + static_cast<size_t>(start_x)) * 4;
        size_t src_idx = (static_cast<size_t>(src_y) * surf_w + static_cast<size_t>(start_x - dst_x)) * 4;

        // Fully opaque layers are processed in spans: runs of opaque pixels are copied, runs of transparent pixels
        // (such as a window's invisible resize band or the empty corners of rounded chrome) are skipped, and only the
        // pixels in between are blended. The result is identical to blending every pixel.
        if (global_opacity == 1.0f) {
            const size_t row_end = src_idx + static_cast<size_t>(end_x - start_x) * 4;
            while (src_idx < row_end) {
                const uint8_t a = src_ptr[src_idx + 3];
                if (a == 255 || a == 0) {
                    size_t run_end = src_idx + 4;
                    while (run_end < row_end && src_ptr[run_end + 3] == a) run_end += 4;
                    if (a == 255) std::memcpy(&dst_ptr[dst_idx], &src_ptr[src_idx], run_end - src_idx);
                    dst_idx += run_end - src_idx;
                    src_idx = run_end;
                    continue;
                }
                const float alpha = a / 255.0f;
                const float inv_alpha = 1.0f - alpha;
                dst_ptr[dst_idx] = static_cast<uint8_t>((src_ptr[src_idx] * alpha) + (dst_ptr[dst_idx] * inv_alpha));
                dst_ptr[dst_idx + 1] = static_cast<uint8_t>((src_ptr[src_idx + 1] * alpha) + (dst_ptr[dst_idx + 1] * inv_alpha));
                dst_ptr[dst_idx + 2] = static_cast<uint8_t>((src_ptr[src_idx + 2] * alpha) + (dst_ptr[dst_idx + 2] * inv_alpha));
                dst_ptr[dst_idx + 3] = std::max(dst_ptr[dst_idx + 3], a);
                src_idx += 4;
                dst_idx += 4;
            }
            continue;
        }

        for (int64_t x = start_x; x < end_x; ++x) {
            uint8_t sr = src_ptr[src_idx];
            uint8_t sg = src_ptr[src_idx + 1];
            uint8_t sb = src_ptr[src_idx + 2];
            uint8_t sa = static_cast<uint8_t>(src_ptr[src_idx + 3] * global_opacity);

            if (sa == 255) {
                dst_ptr[dst_idx] = sr;
                dst_ptr[dst_idx + 1] = sg;
                dst_ptr[dst_idx + 2] = sb;
                dst_ptr[dst_idx + 3] = 255;
            } else if (sa > 0) {
                float alpha = sa / 255.0f;
                float inv_alpha = 1.0f - alpha;
                
                uint8_t dr = dst_ptr[dst_idx];
                uint8_t dg = dst_ptr[dst_idx + 1];
                uint8_t db = dst_ptr[dst_idx + 2];

                dst_ptr[dst_idx] = static_cast<uint8_t>((sr * alpha) + (dr * inv_alpha));
                dst_ptr[dst_idx + 1] = static_cast<uint8_t>((sg * alpha) + (dg * inv_alpha));
                dst_ptr[dst_idx + 2] = static_cast<uint8_t>((sb * alpha) + (db * inv_alpha));
                dst_ptr[dst_idx + 3] = std::max(dst_ptr[dst_idx + 3], sa);
            }
            src_idx += 4;
            dst_idx += 4;
        }
    }
}

} // namespace vynexos::desktop
