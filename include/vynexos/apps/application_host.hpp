#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vynexos::apps {

/// An application the launcher offers.
struct LauncherEntry {
    std::string app_id;
    std::string title;
};

/**
 * @brief Connects applications to windows: what the desktop shell asks for when the user opens or closes one.
 *
 * Lifecycle: open() launches the application through the application manager, creates its window and surface and hands
 * them to the shell; close() takes the window away from the shell, destroys it and terminates the instance. Policy:
 * one instance per application; opening an application that is already open brings its window forward (restoring it
 * if minimized) instead of starting another.
 */
class IApplicationHost {
public:
    virtual ~IApplicationHost() = default;

    /// Opens `app_id` (or brings it forward) and returns its window, or nothing if it is unknown or failed to start.
    virtual std::optional<uint32_t> open(std::string_view app_id) = 0;
    /// Closes the window and terminates its application. Returns false if the window hosts no application.
    virtual bool close(uint32_t window_id) = 0;
    /// The applications the launcher offers, in launcher order.
    [[nodiscard]] virtual std::vector<LauncherEntry> launcher_entries() const = 0;
};

} // namespace vynexos::apps
