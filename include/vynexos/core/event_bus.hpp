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

class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    using EventHandler = std::function<void(std::shared_ptr<const Event>)>;

    // Non-blocking asynchronous dispatch
    virtual void publish(std::shared_ptr<const Event> event) = 0;
    
    // Register typed topic handlers
    virtual void subscribe(const std::string& topic, EventHandler handler) = 0;
};

} // namespace vynexos::core
