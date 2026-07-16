#pragma once
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>

namespace vynexos::apps {

class DemoApp {
public:
    DemoApp(std::shared_ptr<desktop::IWindowManager> wm,
            std::shared_ptr<desktop::ICompositor> compositor,
            std::shared_ptr<desktop::IWidgetToolkit> toolkit);
    
    void launch();
    void update_frame();

private:
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    
    uint32_t m_window_id{0};
    desktop::WindowBuffer m_buffer;
};

} // namespace vynexos::apps
