#pragma once
#include "vynexos/apps/application.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace vynexos::apps {

/**
 * @brief Application Registry and Lifecycle Manager.
 * 
 * Responsibility: Factory registry for discovering available apps, instantiating them, and explicitly driving their lifecycle.
 * Ownership: Owned by the CompositionRoot. Assumes ownership of IApplication instances.
 * Thread Safety: Thread-compatible; multiple threads accessing concurrently require external synchronization.
 */
class IApplicationManager {
public:
    virtual ~IApplicationManager() = default;

    /**
     * @brief Factory signature for creating application instances.
     */
    using AppFactory = std::function<std::unique_ptr<IApplication>()>;

    /**
     * @brief Registers an application factory.
     * @param app_id The unique identifier for the application (e.g., "sys.explorer").
     * @param factory The factory function to instantiate it.
     */
    virtual void register_application(const std::string& app_id, AppFactory factory) = 0;

    /**
     * @brief Instantiates, initializes, and starts a registered application.
     * @param app_id The identifier of the application to launch.
     * @return true if the application was successfully launched.
     */
    virtual bool launch(const std::string& app_id) = 0;

    /**
     * @brief Explicitly shuts down and destroys a specific running application instance.
     * @param instance A pointer to the running instance.
     */
    virtual void terminate(IApplication* instance) = 0;

    /**
     * @brief Explicitly shuts down and destroys all running applications.
     */
    virtual void terminate_all() = 0;

    /**
     * @brief Retrieves a list of all currently running applications.
     * @return A vector of non-owning pointers to the active applications.
     */
    [[nodiscard]] virtual std::vector<IApplication*> get_running_applications() const = 0;
};

} // namespace vynexos::apps
