#pragma once
#include "vynexos/apps/application_manager.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

namespace vynexos::apps {

/**
 * @brief Concrete implementation of IApplicationManager.
 * 
 * Manages the registration, instantiation, and lifetime of application instances.
 * Implements strict ownership transitions via RAII and prevents duplicate registrations.
 */
class ApplicationRegistry final : public IApplicationManager {
public:
    ApplicationRegistry() = default;
    ~ApplicationRegistry() override;

    // Non-copyable, non-movable
    ApplicationRegistry(const ApplicationRegistry&) = delete;
    ApplicationRegistry& operator=(const ApplicationRegistry&) = delete;
    ApplicationRegistry(ApplicationRegistry&&) = delete;
    ApplicationRegistry& operator=(ApplicationRegistry&&) = delete;

    void register_application(const std::string& app_id, AppFactory factory) override;
    bool launch(const std::string& app_id) override;
    void terminate(IApplication* instance) override;
    void terminate_all() override;
    [[nodiscard]] std::vector<IApplication*> get_running_applications() const override;

private:
    std::unordered_map<std::string, AppFactory> m_factories;
    std::vector<std::unique_ptr<IApplication>> m_running_apps;
};

} // namespace vynexos::apps
