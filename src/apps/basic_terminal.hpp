#pragma once
#include "vynexos/apps/terminal.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>
#include <string>
#include <vector>

namespace vynexos::apps {

class BasicTerminal final : public ITerminal {
public:
    BasicTerminal(std::shared_ptr<desktop::IWindowManager> wm,
                  std::shared_ptr<desktop::ICompositor> compositor,
                  std::shared_ptr<desktop::IWidgetToolkit> toolkit);
    
    // Core terminal I/O (ITerminal implementation)
    void write_output(std::string_view text) override;
    void on_input(std::function<void(std::string_view)> callback) override;
    void request_ai_explanation(std::string_view command_context) override;
    void request_ai_suggestion(std::string_view intent) override;
    
    // Window lifecycle
    void launch();
    void update_frame();

private:
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    
    uint32_t m_window_id{0};
    std::shared_ptr<desktop::ISurface> m_surface;
    std::vector<std::string> m_history;
    std::function<void(std::string_view)> m_input_callback;
};

} // namespace vynexos::apps
