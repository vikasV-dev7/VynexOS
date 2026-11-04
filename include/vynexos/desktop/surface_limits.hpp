#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace vynexos::desktop {

// Limits for pixel surfaces (window contents, decorations, panels, the composition framebuffer). Every allocation of
// pixels goes through surface_byte_size(), so the limits live here and nowhere else.
//
// Why these numbers:
//  - kSurfaceBytesPerPixel: surfaces are 8-bit RGBA.
//  - kMaxSurfaceDimension = 16384: the largest texture side that current GPU APIs guarantee (Direct3D 11/12 feature
//    level 11 and common OpenGL/Vulkan drivers), so a surface can always be handed to a GPU backend later. It is also
//    twice the width of an 8K display (7680), so no real window or desktop comes near it.
//  - kMaxSurfacePixels = 64 Mi pixels (256 MiB at 4 bytes per pixel): about twice the pixels of an 8K desktop
//    (7680 x 4320 = 33 Mi), which bounds what one surface can cost while leaving room for a full-screen window on any
//    display VynexOS targets. A 16384 x 16384 surface (1 GiB) is therefore refused even though each side is allowed.
//  - The byte size is computed in 64 bits and must also fit in size_t, so nothing overflows before the allocation on
//    32-bit or 64-bit hosts.
// A width or height of zero is not an allocation request: no surface is created for it.
inline constexpr uint32_t kSurfaceBytesPerPixel = 4;
inline constexpr uint32_t kMaxSurfaceDimension = 16384;
inline constexpr uint64_t kMaxSurfacePixels = uint64_t{64} * 1024 * 1024;

/// The number of bytes a width x height surface needs, or nothing if such a surface must not be allocated (a zero side,
/// a side above kMaxSurfaceDimension, more than kMaxSurfacePixels pixels, or a size not representable in size_t).
[[nodiscard]] constexpr std::optional<std::size_t> surface_byte_size(uint32_t width, uint32_t height) noexcept {
    if (width == 0 || height == 0) return std::nullopt;
    if (width > kMaxSurfaceDimension || height > kMaxSurfaceDimension) return std::nullopt;
    const uint64_t pixels = uint64_t{width} * uint64_t{height}; // at most 2^28: cannot overflow
    if (pixels > kMaxSurfacePixels) return std::nullopt;
    const uint64_t bytes = pixels * kSurfaceBytesPerPixel; // at most 2^28 for the allowed pixel count
    if (bytes > std::numeric_limits<std::size_t>::max()) return std::nullopt;
    return static_cast<std::size_t>(bytes);
}

static_assert(surface_byte_size(kMaxSurfaceDimension, kMaxSurfacePixels / kMaxSurfaceDimension).has_value());
static_assert(!surface_byte_size(kMaxSurfaceDimension, kMaxSurfaceDimension).has_value());
static_assert(!surface_byte_size(0, 1).has_value() && !surface_byte_size(1, 0).has_value());

} // namespace vynexos::desktop
