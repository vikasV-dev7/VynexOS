#include "software_surface.hpp"

namespace vynexos::desktop {

SoftwareSurface::SoftwareSurface(uint32_t width, uint32_t height)
    : m_width(width), m_height(height) {
    m_pixels.resize(m_width * m_height * 4, 0); // Initialize with transparent black
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
