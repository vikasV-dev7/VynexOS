#include "vynexos/hal/viewport.hpp"
#include <algorithm>

namespace vynexos::hal {

Viewport compute_viewport(uint32_t window_width, uint32_t window_height,
                          uint32_t logical_width, uint32_t logical_height) noexcept {
    Viewport vp;
    vp.logical_width = logical_width;
    vp.logical_height = logical_height;
    if (window_width == 0 || window_height == 0 || logical_width == 0 || logical_height == 0) {
        return vp;
    }

    const uint64_t ww = window_width;
    const uint64_t wh = window_height;
    const uint64_t lw = logical_width;
    const uint64_t lh = logical_height;

    if (ww * lh <= wh * lw) {
        // Width-limited: fill the width, bars above and below.
        vp.width = window_width;
        vp.height = static_cast<uint32_t>(std::min<uint64_t>(wh, (ww * lh + lw / 2) / lw));
    } else {
        // Height-limited: fill the height, bars left and right.
        vp.height = window_height;
        vp.width = static_cast<uint32_t>(std::min<uint64_t>(ww, (wh * lw + lh / 2) / lh));
    }
    vp.x = static_cast<int32_t>((window_width - vp.width) / 2);
    vp.y = static_cast<int32_t>((window_height - vp.height) / 2);
    return vp;
}

LogicalPoint window_to_logical(const Viewport& viewport, int32_t window_x, int32_t window_y) noexcept {
    if (viewport.width == 0 || viewport.height == 0 ||
        viewport.logical_width == 0 || viewport.logical_height == 0) {
        return {0, 0};
    }

    const int64_t rel_x = std::clamp<int64_t>(static_cast<int64_t>(window_x) - viewport.x, 0,
                                              static_cast<int64_t>(viewport.width) - 1);
    const int64_t rel_y = std::clamp<int64_t>(static_cast<int64_t>(window_y) - viewport.y, 0,
                                              static_cast<int64_t>(viewport.height) - 1);

    return {static_cast<int32_t>(rel_x * viewport.logical_width / viewport.width),
            static_cast<int32_t>(rel_y * viewport.logical_height / viewport.height)};
}

} // namespace vynexos::hal

namespace vynexos::hal {

int32_t window_units_to_pixels(int32_t coord, uint32_t window_extent, uint32_t pixel_extent) noexcept {
    if (window_extent == 0 || pixel_extent == 0) {
        return coord;
    }
    return static_cast<int32_t>(static_cast<int64_t>(coord) * pixel_extent / window_extent);
}

} // namespace vynexos::hal
