#pragma once
#include <string>
#include <string_view>
#include <expected>
#include <memory>
#include <vector>

namespace vynexos::core {

enum class ServiceState {
    Stopped,
    Starting,
    Running,
    Failed
};

class IService {
public:
    virtual ~IService() = default;
    
    virtual std::string_view get_name() const = 0;
    virtual std::expected<void, std::string> start() = 0;
    virtual void stop() = 0;
    virtual ServiceState get_state() const = 0;
};

class IServiceManager {
public:
    virtual ~IServiceManager() = default;
    
    virtual void register_service(std::shared_ptr<IService> service) = 0;
    virtual std::expected<void, std::string> start_all() = 0;
    virtual void stop_all() = 0;
};

} // namespace vynexos::core
