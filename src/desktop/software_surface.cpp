#include "software_surface.hpp"
#include "vynexos/desktop/surface_limits.hpp"
#include <stdexcept>
#include <string>

namespace vynexos::desktop {

SoftwareSurface::SoftwareSurface(uint32_t width, uint32_t height)
    : m_width(width), m_height(height) {
    if (width == 0 || height == 0) return; // empty: no pixels
    const auto bytes = surface_byte_size(width, height);
    if (!bytes) throw std::length_error("SoftwareSurface: " + std::to_string(width) + "x" + std::to_string(height) + " exceeds the surface limits");
    m_pixels.resize(*bytes, 0); // Initialize with transparent black
}

uint32_t SoftwareSurface::width() const {
    return m_width;
}

uint32_t SoftwareSurface::height() const {
    return m_height;
}

std::span<uint8_t> SoftwareSurface::map_pixels() {
    return std::span<uint8_t>(m_pixels);
}

void SoftwareSurface::unmap_pixels() {
    // No-op for software surface, as drawing happens directly to the mapped vector.
}

} // namespace vynexos::desktop
