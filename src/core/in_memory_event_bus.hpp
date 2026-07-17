#pragma once
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/task_scheduler.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace vynexos::core {

class InMemoryEventBus final : public IEventBus {
public:
    explicit InMemoryEventBus(ITaskScheduler& scheduler) 
        : m_scheduler(scheduler) {}

    ~InMemoryEventBus() override = default;

    void subscribe(const std::string& topic, EventHandler handler) override {
        std::unique_lock lock(m_sub_mutex);
        m_subscribers[topic].push_back(std::move(handler));
    }

    void publish(std::shared_ptr<const Event> event) override {
        bool accepted = m_scheduler.enqueue([this, evt = std::move(event)](const ExecutionContext&) {
            std::shared_lock lock(m_sub_mutex);
            auto it = m_subscribers.find(evt->topic);
            if (it != m_subscribers.end()) {
                for (const auto& handler : it->second) {
                    handler(evt);
                }
            }
        });
        
        if (!accepted) {
            // Task scheduler is shutting down. The event is intentionally discarded.
        }
    }

private:
    ITaskScheduler& m_scheduler;
    std::shared_mutex m_sub_mutex;
    std::unordered_map<std::string, std::vector<EventHandler>> m_subscribers;
};

} // namespace vynexos::core
