#pragma once
#include <memory>
#include <string_view>
#include <vector>
#include <cstdint>
#include "vynexos/desktop/desktop_types.hpp"


namespace vynexos::desktop {

struct WindowGeometry {
    int32_t x, y;
    uint32_t width, height;
};

class IWindowManager {
public:
    virtual ~IWindowManager() = default;
    
    // Window lifecycle
    virtual uint32_t create_window(std::string_view title, WindowGeometry geometry) = 0;
    virtual void destroy_window(uint32_t window_id) = 0;
    
    // Focus and layout
    virtual void focus_window(uint32_t window_id) = 0;
    virtual void move_window(uint32_t window_id, int32_t x, int32_t y) = 0;
    virtual void resize_window(uint32_t window_id, uint32_t width, uint32_t height) = 0;
    
    // Surface and visibility
    virtual void set_window_surface(uint32_t window_id, std::shared_ptr<ISurface> surface) = 0;
    virtual void set_window_visibility(uint32_t window_id, bool visible) = 0;
    
    // Returns window IDs ordered back-to-front for the compositor
    virtual std::vector<uint32_t> get_windows_z_ordered() const = 0;

    // Scene Orchestration
    virtual SceneGraph build_scene() const = 0;
};

} // namespace vynexos::desktop
