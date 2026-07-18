#pragma once
#include <memory>
#include <vector>
#include <expected>
#include <cstdint>
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

class ICompositor {
public:
    virtual ~ICompositor() = default;
    
    // New Interface
    virtual std::shared_ptr<ISurface> create_surface(uint32_t width, uint32_t height) = 0;
    virtual std::expected<void, DisplayError> render_frame(const SceneGraph& scene) = 0;
    

};

} // namespace vynexos::desktop
