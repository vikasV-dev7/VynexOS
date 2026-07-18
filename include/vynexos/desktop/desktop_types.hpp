#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <span>

namespace vynexos::desktop {

struct Rect {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

// Represents a renderable graphics surface.
class ISurface {
public:
    virtual ~ISurface() = default;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    
    // Polymorphic memory access for drawing
    virtual std::span<uint8_t> map_pixels() = 0;
    virtual void unmap_pixels() = 0;
};

struct RenderLayer {
    Rect bounds;
    Rect clip;
    float opacity{1.0f};
    bool visible{true};
    std::shared_ptr<ISurface> surface;
    uint32_t window_id{0};
};

using SceneGraph = std::vector<RenderLayer>;

} // namespace vynexos::desktop
