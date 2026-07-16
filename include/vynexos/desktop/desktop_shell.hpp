#pragma once
#include <memory>
#include <vector>
#include <cstdint>

namespace vynexos::desktop {

class IDesktopShell {
public:
    virtual ~IDesktopShell() = default;
    
    // Shell components
    virtual void toggle_application_launcher() = 0;
    virtual void update_taskbar() = 0;
    virtual void lock_screen() = 0;
    
    // Exposes the shell's overlay windows (panels, notifications) to the compositor
    virtual std::vector<uint32_t> get_overlay_window_ids() const = 0;
};

} // namespace vynexos::desktop
