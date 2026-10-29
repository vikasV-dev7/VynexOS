#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

class ISurface;

struct TaskbarEntry {
    uint32_t window_id;
    std::string title;
    WindowState state;
    bool is_active;
    bool attention_requested{false};
};

class ITaskbar {
public:
    virtual ~ITaskbar() = default;
    
    // Updates internal state based on current desktop windows
    // Returns true if the taskbar entries have changed and require a redraw
    virtual bool synchronize(const std::vector<TaskbarEntry>& entries) = 0;
    
    // Draws the taskbar to the provided surface
    virtual void draw(std::shared_ptr<ISurface> surface) = 0;
    
    // Returns the window_id corresponding to the clicked area, if any
    virtual std::optional<uint32_t> get_clicked_window(int32_t x, int32_t y) const = 0;
};

} // namespace vynexos::desktop
