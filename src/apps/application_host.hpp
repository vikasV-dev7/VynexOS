#pragma once
#include "vynexos/apps/application_host.hpp"
#include "vynexos/apps/application_manager.hpp"
#include "vynexos/apps/desktop_shell.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace vynexos::apps {

/**
 * Ownership: the application manager owns every instance; the window manager owns windows; the shell owns each running
 * application's surface (shared with the window manager for compositing). The host owns none of them: it records which
 * window belongs to which instance and keeps those three consistent. An instance is reachable through the host only
 * while its window exists, and its window only while the instance runs.
 */
class ApplicationHost final : public IApplicationHost {
public:
    struct Descriptor {
        std::string app_id;
        std::string title;                  ///< window title and launcher label
        desktop::WindowGeometry geometry;   ///< client area of a newly opened window
        bool in_launcher{true};
    };

    ApplicationHost(std::shared_ptr<IApplicationManager> manager, std::shared_ptr<desktop::IWindowManager> wm,
                    std::shared_ptr<desktop::ICompositor> compositor, std::shared_ptr<IDesktopShell> shell);

    /// Makes an application available (to open() and, if `in_launcher`, the launcher). Throws if the id is taken.
    void register_application(Descriptor descriptor, IApplicationManager::AppFactory factory);

    std::optional<uint32_t> open(std::string_view app_id) override;
    bool close(uint32_t window_id) override;
    [[nodiscard]] std::vector<LauncherEntry> launcher_entries() const override;

    /// The open window of an application, if it is open.
    [[nodiscard]] std::optional<uint32_t> window_of(std::string_view app_id) const;
    /// The instance hosted in a window (owned by the application manager), or nullptr.
    [[nodiscard]] IApplication* application_in(uint32_t window_id) const;
    /// Closes every hosted application (shutdown).
    void close_all();

private:
    struct Hosted {
        std::string app_id;
        IApplication* instance;
        uint32_t window_id;
    };

    std::shared_ptr<IApplicationManager> m_manager;
    std::shared_ptr<desktop::IWindowManager> m_wm;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<IDesktopShell> m_shell;
    std::vector<Descriptor> m_descriptors;
    std::vector<Hosted> m_hosted;
};

} // namespace vynexos::apps
