#include "application_host.hpp"
#include "vynexos/desktop/running_application.hpp"
#include <algorithm>
#include <stdexcept>

namespace vynexos::apps {

ApplicationHost::ApplicationHost(std::shared_ptr<IApplicationManager> manager, std::shared_ptr<desktop::IWindowManager> wm,
                                 std::shared_ptr<desktop::ICompositor> compositor, std::shared_ptr<IDesktopShell> shell)
    : m_manager(std::move(manager)), m_wm(std::move(wm)), m_compositor(std::move(compositor)), m_shell(std::move(shell)) {}

void ApplicationHost::register_application(Descriptor descriptor, IApplicationManager::AppFactory factory) {
    m_manager->register_application(descriptor.app_id, std::move(factory)); // throws on a duplicate id
    m_descriptors.push_back(std::move(descriptor));
}

std::optional<uint32_t> ApplicationHost::open(std::string_view app_id) {
    const auto descriptor = std::find_if(m_descriptors.begin(), m_descriptors.end(), [&](const Descriptor& d) { return d.app_id == app_id; });
    if (descriptor == m_descriptors.end()) return std::nullopt;

    if (const auto window = window_of(app_id)) {
        // Already open: bring it forward.
        if (m_wm->get_window_state(*window) == desktop::WindowState::Minimized) {
            m_wm->set_window_state(*window, m_wm->get_previous_window_state(*window));
            const auto bounds = m_wm->get_decorated_window_bounds(*window);
            m_compositor->invalidate_region(bounds);
        }
        m_shell->focus_window(*window);
        return window;
    }

    IApplication* instance = m_manager->launch_instance(descriptor->app_id);
    if (!instance) return std::nullopt;

    const desktop::WindowGeometry geometry = descriptor->geometry;
    auto surface = m_compositor->create_surface(geometry.width, geometry.height);
    if (!surface) {
        m_manager->terminate(instance); // a window size no surface may have: nothing is opened
        return std::nullopt;
    }
    const uint32_t window = m_wm->create_window(descriptor->title, geometry);
    m_wm->set_window_surface(window, surface);

    desktop::RunningApplication running;
    running.app = instance;
    running.window_id = window;
    running.surface = std::move(surface);
    running.visible = true;
    m_shell->add_running_application(std::move(running));
    m_hosted.push_back({descriptor->app_id, instance, window});
    m_shell->focus_window(window);
    return window;
}

bool ApplicationHost::close(uint32_t window_id) {
    const auto hosted = std::find_if(m_hosted.begin(), m_hosted.end(), [&](const Hosted& h) { return h.window_id == window_id; });
    if (hosted == m_hosted.end()) return false;
    IApplication* const instance = hosted->instance;
    m_hosted.erase(hosted);

    // Stop delivering frames and input first, then take the window off the screen, then end the instance.
    const auto bounds = m_wm->get_decorated_window_bounds(window_id);
    m_shell->remove_running_application(window_id);
    m_wm->destroy_window(window_id);
    if (bounds.width > 0 && bounds.height > 0) m_compositor->invalidate_region(bounds);
    m_manager->terminate(instance);
    return true;
}

std::vector<LauncherEntry> ApplicationHost::launcher_entries() const {
    std::vector<LauncherEntry> entries;
    for (const auto& d : m_descriptors) {
        if (d.in_launcher) entries.push_back({d.app_id, d.title});
    }
    return entries;
}

std::optional<uint32_t> ApplicationHost::window_of(std::string_view app_id) const {
    const auto hosted = std::find_if(m_hosted.begin(), m_hosted.end(), [&](const Hosted& h) { return h.app_id == app_id; });
    if (hosted == m_hosted.end()) return std::nullopt;
    return hosted->window_id;
}

IApplication* ApplicationHost::application_in(uint32_t window_id) const {
    const auto hosted = std::find_if(m_hosted.begin(), m_hosted.end(), [&](const Hosted& h) { return h.window_id == window_id; });
    return hosted == m_hosted.end() ? nullptr : hosted->instance;
}

void ApplicationHost::close_all() {
    while (!m_hosted.empty()) close(m_hosted.back().window_id);
}

} // namespace vynexos::apps
