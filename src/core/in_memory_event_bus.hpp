#pragma once
#include "vynexos/core/event_bus.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <string>

namespace vynexos::core {

class InMemoryEventBus final : public IEventBus {
public:
    InMemoryEventBus() {
        m_worker = std::thread([this]() { worker_loop(); });
    }

    ~InMemoryEventBus() override {
        m_stop = true;
        m_cv.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    void subscribe(const std::string& topic, EventHandler handler) override {
        std::unique_lock lock(m_sub_mutex);
        m_subscribers[topic].push_back(std::move(handler));
    }

    void publish(std::shared_ptr<const Event> event) override {
        std::unique_lock lock(m_queue_mutex);
        m_queue.push({event->topic, event});
        m_cv.notify_one();
    }

private:
    void worker_loop() {
        while (!m_stop) {
            std::pair<std::string, std::shared_ptr<const Event>> task;
            {
                std::unique_lock lock(m_queue_mutex);
                m_cv.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
                
                if (m_stop && m_queue.empty()) break;
                
                task = std::move(m_queue.front());
                m_queue.pop();
            }

            // Dispatch to subscribers
            std::shared_lock lock(m_sub_mutex);
            auto it = m_subscribers.find(task.first);
            if (it != m_subscribers.end()) {
                for (const auto& handler : it->second) {
                    handler(task.second);
                }
            }
        }
    }

    std::shared_mutex m_sub_mutex;
    std::unordered_map<std::string, std::vector<EventHandler>> m_subscribers;

    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    std::queue<std::pair<std::string, std::shared_ptr<const Event>>> m_queue;
    
    std::thread m_worker;
    std::atomic<bool> m_stop{false};
};

} // namespace vynexos::core
