#pragma once

namespace vynexos::apps {

class IDesktopShell {
public:
    virtual ~IDesktopShell() = default;
    
    virtual void launch() = 0;
    virtual void update_frame() = 0;
};

} // namespace vynexos::apps
