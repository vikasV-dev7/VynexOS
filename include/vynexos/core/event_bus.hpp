#pragma once
#include <functional>
#include <string>
#include <memory>
#include <any>

namespace vynexos::core {

struct Event {
    std::string topic;
    std::any payload; // Type-erased payload, consumers can std::any_cast
};

/**
 * @interface IEventBus
 * @brief Provides a decentralized pub-sub communication layer for loosely-coupled components.
 * 
 * ### Concurrency and Execution Contract
 * - **Thread Safety:** `publish()` and `subscribe()` are fully thread-safe and non-blocking. Two `publish()` calls can execute concurrently from different threads.
 * - **Global Event Ordering:** NOT guaranteed. Events are dispatched asynchronously. The chronological order of `publish()` calls does not dictate the execution order of handlers.
 * - **Publisher Event Ordering:** NOT guaranteed. Sequential `publish(A)` followed by `publish(B)` from the same thread may result in B being processed before A due to underlying thread-pool scheduling.
 * - **Handler Concurrency:** Handlers for the *same* event instance are executed sequentially (in a single worker context). Handlers for *different* event instances will execute concurrently.
 * - **Delivery Guarantees:** "At-most-once" delivery per active subscriber. If an event is published while the system is shutting down, it may be discarded.
 * @Purpose The central nervous system for decoupled inter-module communication.
 * @Responsibilities Subscribes handlers, dispatches events synchronously/asynchronously, manages topic routing.
 * @Ownership Shared ownership via Dependency Injection to all major system services.
 * @Lifetime Lives for the entire duration of the `CompositionRoot::run()` loop.
 * @ThreadSafety Subscribers must be added strictly on the main thread. Publish is thread-safe.
 * @FailureHandling If an event handler throws, the EventBus catches and logs the exception but continues dispatching.
 * @ExtensionPoints Custom event types can be derived from `SystemEvent`.
 */
class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    using EventHandler = std::function<void(std::shared_ptr<const Event>)>;

    /**
     * @brief Dispatches an event asynchronously.
     * @param event The event payload to dispatch.
     */
    virtual void publish(std::shared_ptr<const Event> event) = 0;
    
    /**
     * @brief Registers a handler for a specific topic.
     * @param topic The string identifier for the topic.
     * @param handler The callback executed when an event is dispatched. Must be thread-safe.
     */
    virtual void subscribe(const std::string& topic, EventHandler handler) = 0;
};

} // namespace vynexos::core
