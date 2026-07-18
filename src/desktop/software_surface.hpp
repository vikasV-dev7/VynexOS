#pragma once
#include "vynexos/desktop/desktop_types.hpp"
#include <vector>

namespace vynexos::desktop {

class SoftwareSurface final : public ISurface {
public:
    SoftwareSurface(uint32_t width, uint32_t height);
    ~SoftwareSurface() override = default;

    uint32_t width() const override;
    uint32_t height() const override;

    std::span<uint8_t> map_pixels() override;
    void unmap_pixels() override;

private:
    uint32_t m_width;
    uint32_t m_height;
    std::vector<uint8_t> m_pixels;
};

} // namespace vynexos::desktop
