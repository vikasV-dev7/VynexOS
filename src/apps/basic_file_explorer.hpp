#pragma once
#include "vynexos/apps/application.hpp"
#include "vynexos/core/virtual_file_system.hpp"
#include "vynexos/core/virtual_path.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace vynexos::apps {

/**
 * @brief Represents a file or directory entry in the UI.
 */
struct DirectoryEntry {
    std::string display_name;
    core::VirtualPath full_path;
    core::FileStat stat;
};

class BasicFileExplorer final : public IApplication {
public:
    // -------------------------------------------------------------------------
    BasicFileExplorer(std::shared_ptr<core::IVirtualFileSystem> vfs,
                      std::shared_ptr<desktop::IWidgetToolkit> toolkit);

    // IApplication Implementation
    [[nodiscard]] bool initialize() override;
    void start() override;
    void update(double delta_time_ms) override;
    bool handle_event(const vynexos::core::Event& event) override;
    void render(vynexos::desktop::ISurface& target_surface) override;
    void shutdown() override;
    [[nodiscard]] std::string get_name() const noexcept override;

    // Test accessors (optional, for verification)
    const core::VirtualPath& get_current_path() const { return m_current_path; }
    const std::vector<DirectoryEntry>& get_entries() const { return m_entries; }
    std::optional<size_t> get_selected_index() const { return m_selected_index; }

private:
    // Navigation Helpers
    bool refresh_current_directory();
    bool navigate_to(const core::VirtualPath& path);
    void navigate_up();

private:
    std::shared_ptr<core::IVirtualFileSystem> m_vfs;
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    
    // Application State
    core::VirtualPath m_current_path;
    std::vector<DirectoryEntry> m_entries;
    std::optional<size_t> m_selected_index;
};

} // namespace vynexos::apps
