#pragma once
#include <cstdint>

namespace vynexos::hal {

/**
 * @brief Destination rectangle (in window pixels) that the logical desktop is drawn into.
 *
 * The logical desktop is scaled uniformly (aspect ratio preserved) and centered,
 * leaving letterbox/pillarbox bars on the unused axis.
 */
struct Viewport {
    int32_t x{0};
    int32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t logical_width{0};
    uint32_t logical_height{0};
};

struct LogicalPoint {
    int32_t x;
    int32_t y;
};

/// Computes the aspect-preserving, centered viewport for a window of the given pixel size.
/// A zero-sized window or logical size yields an empty viewport.
[[nodiscard]] Viewport compute_viewport(uint32_t window_width, uint32_t window_height,
                                        uint32_t logical_width, uint32_t logical_height) noexcept;

/// Maps a window-pixel position to logical desktop coordinates.
/// Positions inside the letterbox bars or outside the window are clamped to the nearest
/// desktop edge, so pointer capture (drag/resize) keeps working past the viewport.
[[nodiscard]] LogicalPoint window_to_logical(const Viewport& viewport, int32_t window_x, int32_t window_y) noexcept;

/// Converts a coordinate in window units to renderer pixels. The two differ on high-DPI
/// displays (e.g. a 1024-unit-wide window rendered into 1536 pixels scales by 1.5).
/// Returns the input unchanged if either extent is zero.
[[nodiscard]] int32_t window_units_to_pixels(int32_t coord, uint32_t window_extent, uint32_t pixel_extent) noexcept;

/// Converts raw window coordinates (as reported by the OS/SDL) to logical desktop coordinates.
class IPointerMapper {
public:
    virtual ~IPointerMapper() = default;
    [[nodiscard]] virtual LogicalPoint to_logical(int32_t window_x, int32_t window_y) const = 0;
};

} // namespace vynexos::hal
