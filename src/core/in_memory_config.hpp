#pragma once
#include "vynexos/core/config_manager.hpp"
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace vynexos::core {

class InMemoryConfigManager final : public IConfigManager {
public:
    std::expected<std::any, ConfigError> get_value(std::string_view key) const override {
        std::shared_lock lock(m_mutex);
        auto it = m_store.find(std::string(key));
        if (it != m_store.end()) {
            return it->second;
        }
        return std::unexpected(ConfigError::KeyNotFound);
    }

    std::expected<void, ConfigError> set_value(std::string_view key, std::any value) override {
        std::unique_lock lock(m_mutex);
        m_store[std::string(key)] = std::move(value);
        return {};
    }

    bool has_key(std::string_view key) const override {
        std::shared_lock lock(m_mutex);
        return m_store.contains(std::string(key));
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::any> m_store;
};

} // namespace vynexos::core
