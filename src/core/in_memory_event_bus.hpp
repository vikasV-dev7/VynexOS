#pragma once
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/task_scheduler.hpp"
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace vynexos::core {

/**
 * Lifetime model. Asynchronous deliveries are tasks on the scheduler, and a task may run after whoever published it, any
 * subscriber, or the bus itself is gone. So:
 *  - a queued task holds the bus's shared State (never the bus): it stays valid however long the task waits;
 *  - handlers run under a shared lock on that State; unsubscribe() and close() take it exclusively, so they wait for
 *    running handlers and, once they return, no handler of theirs runs again;
 *  - destroying the bus closes the State: queued deliveries find it closed and do nothing.
 * The bus uses its scheduler for publish(), so the scheduler must outlive the bus.
 */
class InMemoryEventBus final : public IEventBus {
public:
    explicit InMemoryEventBus(ITaskScheduler& scheduler)
        : m_scheduler(scheduler), m_state(std::make_shared<State>()) {}

    ~InMemoryEventBus() override { close(); }

    InMemoryEventBus(const InMemoryEventBus&) = delete;
    InMemoryEventBus& operator=(const InMemoryEventBus&) = delete;

    SubscriptionId subscribe(const std::string& topic, EventHandler handler) override {
        std::unique_lock lock(m_state->mutex);
        if (m_state->closed) return 0;
        const SubscriptionId id = ++m_state->next_id;
        m_state->subscribers[topic].push_back({id, std::move(handler)});
        return id;
    }

    void unsubscribe(SubscriptionId id) override {
        if (id == 0) return;
        std::unique_lock lock(m_state->mutex); // waits for every handler running under the shared lock
        for (auto& [topic, subscriptions] : m_state->subscribers) {
            std::erase_if(subscriptions, [id](const Subscription& s) { return s.id == id; });
        }
    }

    void publish(std::shared_ptr<const Event> event) override {
        if (m_state->closed_flag.load(std::memory_order_acquire)) return;
        const bool accepted = m_scheduler.enqueue([state = m_state, evt = std::move(event)](const ExecutionContext&) { deliver(*state, evt); });
        if (!accepted) {
            // Task scheduler is shutting down. The event is intentionally discarded.
        }
    }

    void dispatch_immediate(std::shared_ptr<const Event> event) override { deliver(*m_state, event); }

    /// Stops all delivery for good: queued asynchronous events are dropped, later ones go nowhere, every handler is
    /// released. Waits for handlers already running. Called by the destructor; calling it again does nothing.
    void close() {
        std::unique_lock lock(m_state->mutex);
        m_state->closed = true;
        m_state->closed_flag.store(true, std::memory_order_release);
        m_state->subscribers.clear();
    }

    /// The number of live subscriptions to a topic (diagnostics and tests).
    [[nodiscard]] size_t subscriber_count(const std::string& topic) const {
        std::shared_lock lock(m_state->mutex);
        const auto it = m_state->subscribers.find(topic);
        return it == m_state->subscribers.end() ? 0 : it->second.size();
    }

private:
    struct Subscription {
        SubscriptionId id;
        EventHandler handler;
    };
    struct State {
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, std::vector<Subscription>> subscribers;
        SubscriptionId next_id{0};
        bool closed{false};                  // guarded by mutex
        std::atomic<bool> closed_flag{false}; // the same, readable without the lock (publish fast path)
    };

    static void deliver(const State& state, const std::shared_ptr<const Event>& event) {
        std::shared_lock lock(state.mutex);
        if (state.closed) return;
        const auto it = state.subscribers.find(event->topic);
        if (it == state.subscribers.end()) return;
        for (const auto& s : it->second) s.handler(event);
    }

    ITaskScheduler& m_scheduler;
    std::shared_ptr<State> m_state;
};

} // namespace vynexos::core
