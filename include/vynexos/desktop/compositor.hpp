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
    
    /// A new surface of width x height pixels, or nullptr if that size must not be allocated (see surface_limits.hpp:
    /// a zero side, a side above kMaxSurfaceDimension, or more than kMaxSurfacePixels). Callers keep what they had when
    /// it returns nullptr.
    virtual std::shared_ptr<ISurface> create_surface(uint32_t width, uint32_t height) = 0;
    virtual void invalidate_region(const Rect& rect) = 0;
    virtual std::expected<void, DisplayError> render_frame(const SceneGraph& scene) = 0;

    /// Size of the logical desktop the compositor renders; UI layout derives from this.
    virtual hal::DisplaySize logical_size() const { return hal::kDefaultDisplaySize; }
    

};

} // namespace vynexos::desktop
