#pragma once
#include "vynexos/core/service_manager.hpp"
#include "vynexos/core/logger.hpp"
#include <vector>
#include <mutex>
#include <memory>

namespace vynexos::core {

class BasicServiceManager final : public IServiceManager {
public:
    explicit BasicServiceManager(std::shared_ptr<ILogger> logger) 
        : m_logger(std::move(logger)) {}

    void register_service(std::shared_ptr<IService> service) override {
        std::lock_guard lock(m_mutex);
        m_services.push_back(std::move(service));
    }

    std::expected<void, std::string> start_all() override {
        std::lock_guard lock(m_mutex);
        for (auto& svc : m_services) {
            m_logger->info("Starting service: {}", svc->get_name());
            if (auto res = svc->start(); !res) {
                m_logger->error("Failed to start service {}: {}", svc->get_name(), res.error());
                return res;
            }
        }
        return {};
    }

    void stop_all() override {
        std::lock_guard lock(m_mutex);
        // Stop in reverse order
        for (auto it = m_services.rbegin(); it != m_services.rend(); ++it) {
            m_logger->info("Stopping service: {}", (*it)->get_name());
            (*it)->stop();
        }
    }

private:
    std::shared_ptr<ILogger> m_logger;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<IService>> m_services;
};

} // namespace vynexos::core
