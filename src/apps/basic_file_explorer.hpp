#pragma once
#include "vynexos/apps/file_explorer.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>
#include <vector>

namespace vynexos::apps {

class BasicFileExplorer final : public IFileExplorer {
public:
    BasicFileExplorer(std::shared_ptr<desktop::IWindowManager> wm,
                      std::shared_ptr<desktop::ICompositor> compositor,
                      std::shared_ptr<desktop::IWidgetToolkit> toolkit);
    
    // Core file explorer APIs
    std::vector<FileNode> list_directory(std::string_view path) override;
    void semantic_search_async(std::string_view query, std::function<void(std::vector<FileNode>)> callback) override;
    
    // Window lifecycle
    void launch();
    void update_frame();

private:
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    
    uint32_t m_window_id{0};
    std::shared_ptr<desktop::ISurface> m_surface;
};

} // namespace vynexos::apps
