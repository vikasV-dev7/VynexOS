#pragma once

namespace vynexos::core {

/**
 * @brief Minimal execution context passed to background tasks.
 *
 * Designed for future extensibility (e.g., cancellation tokens, priority levels).
 */
struct ExecutionContext {
    // Placeholder for Sprint 14
    // Future: std::stop_token, priority ID, etc.
};

} // namespace vynexos::core
