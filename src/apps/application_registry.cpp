#include "vynexos/apps/application_registry.hpp"
#include <algorithm>
#include <stdexcept>

namespace vynexos::apps {

ApplicationRegistry::~ApplicationRegistry() {
    terminate_all();
}

void ApplicationRegistry::register_application(const std::string& app_id, AppFactory factory) {
    if (m_factories.contains(app_id)) {
        throw std::invalid_argument("Application already registered: " + app_id);
    }
    m_factories[app_id] = std::move(factory);
}

bool ApplicationRegistry::launch(const std::string& app_id) {
    auto it = m_factories.find(app_id);
    if (it == m_factories.end()) {
        return false;
    }

    auto instance = it->second();
    if (!instance) {
        return false;
    }

    if (!instance->initialize()) {
        return false; // instance is safely destroyed here automatically
    }

    instance->start();
    m_running_apps.push_back(std::move(instance));
    return true;
}

void ApplicationRegistry::terminate(IApplication* instance) {
    if (!instance) return;

    auto it = std::find_if(m_running_apps.begin(), m_running_apps.end(),
        [instance](const std::unique_ptr<IApplication>& ptr) {
            return ptr.get() == instance;
        });

    if (it != m_running_apps.end()) {
        (*it)->shutdown();
        m_running_apps.erase(it); // Erase from vector destroys the unique_ptr -> calls destructor
    }
}

void ApplicationRegistry::terminate_all() {
    for (auto& app : m_running_apps) {
        app->shutdown();
    }
    m_running_apps.clear();
}

std::vector<IApplication*> ApplicationRegistry::get_running_applications() const {
    std::vector<IApplication*> result;
    result.reserve(m_running_apps.size());
    for (const auto& app : m_running_apps) {
        result.push_back(app.get());
    }
    return result;
}

} // namespace vynexos::apps
