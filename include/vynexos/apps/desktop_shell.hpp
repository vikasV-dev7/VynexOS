#pragma once
#include <optional>
#include <memory>
#include "vynexos/desktop/running_application.hpp"
#include "vynexos/desktop/taskbar.hpp"

namespace vynexos::apps {

class IDesktopShell {
public:
    virtual ~IDesktopShell() = default;
    
    virtual void launch() = 0;
    virtual void update_frame() = 0;
    
    // Runtime Ownership
    virtual void add_running_application(desktop::RunningApplication&& app) = 0;
    virtual void remove_running_application(uint32_t window_id) = 0;
    virtual void clear_running_applications() = 0;
    virtual size_t running_application_count() const = 0;
    
    // Focus Management
    virtual void focus_window(uint32_t window_id) = 0;
    virtual void clear_focus() = 0;
    virtual std::optional<uint32_t> focused_window() const = 0;
    virtual desktop::RunningApplication* focused_application() = 0;
    virtual bool has_focus() const = 0;
    
    // Component Access
    virtual std::shared_ptr<desktop::ITaskbar> taskbar() = 0;
};

} // namespace vynexos::apps
