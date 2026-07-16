#pragma once
#include <memory>
#include <vector>
#include <expected>
#include <cstdint>
#include "vynexos/desktop/display_abstraction.hpp"

namespace vynexos::desktop {

struct WindowBuffer {
    uint32_t id;
    int32_t x, y;
    uint32_t width, height;
    uint32_t z_index;
    std::vector<uint8_t> pixels;
};

class ICompositor {
public:
    virtual ~ICompositor() = default;
    
    // Submits a window's updated buffer for compositing
    virtual void submit_buffer(const WindowBuffer& buffer) = 0;
    
    // Composites all z-ordered windows and pushes to DisplayAbstraction
    virtual std::expected<void, DisplayError> render_frame() = 0;
};

} // namespace vynexos::desktop
