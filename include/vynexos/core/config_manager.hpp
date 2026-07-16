#pragma once
#include <string>
#include <string_view>
#include <expected>
#include <any>

namespace vynexos::core {

enum class ConfigError {
    KeyNotFound,
    TypeMismatch,
    AccessDenied
};

class IConfigManager {
public:
    virtual ~IConfigManager() = default;

    virtual std::expected<std::any, ConfigError> get_value(std::string_view key) const = 0;
    virtual std::expected<void, ConfigError> set_value(std::string_view key, std::any value) = 0;
    virtual bool has_key(std::string_view key) const = 0;
};

} // namespace vynexos::core
