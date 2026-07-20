#pragma once
#include <string>

namespace vynexos::core {
    struct Event;
}

namespace vynexos::desktop {
    class ISurface;
}

namespace vynexos::apps {

/**
 * @brief Represents an interactive user-space application within the VynexOS runtime.
 * 
 * Responsibility: Handle internal application state, process input events, and render UI.
 * Ownership: Owned and managed strictly by the IApplicationManager.
 * Lifetime: Explicitly governed by the ApplicationManager.
 * Thread Safety: Single-threaded by design. Not guaranteed to be thread-safe unless implemented as such.
 */
class IApplication {
public:
    virtual ~IApplication() = default;

    /**
     * @brief Performs one-time setup and resource allocation.
     * @return true if initialization succeeded, false to abort launch.
     */
    [[nodiscard]] virtual bool initialize() = 0;

    /**
     * @brief Signals the application to become active.
     */
    virtual void start() = 0;

    /**
     * @brief Called periodically to update internal logic (e.g., animations, timers).
     * @param delta_time_ms Time elapsed since the last update.
     */
    virtual void update(double delta_time_ms) = 0;

    /**
     * @brief Delivers a system or input event to the application.
     * @param event The abstract domain event payload.
     * @return true if the event was consumed.
     */
    virtual bool handle_event(const vynexos::core::Event& event) = 0;

    /**
     * @brief Instructs the application to draw itself to its assigned surface.
     * @param target_surface The hardware-agnostic surface provided by the presentation layer.
     */
    virtual void render(vynexos::desktop::ISurface& target_surface) = 0;

    /**
     * @brief Signals the application to explicitly release resources prior to destruction.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Returns the unique identifier or human-readable name of the application.
     */
    [[nodiscard]] virtual std::string get_name() const noexcept = 0;
};

} // namespace vynexos::apps
