#pragma once
#include <string_view>
#include <format>
#include <memory>
#include <string>

namespace vynexos::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
    Critical
};

class ILogger {
public:
    virtual ~ILogger() = default;
    
    // Configuration
    virtual void set_log_level(LogLevel level) = 0;
    [[nodiscard]] virtual LogLevel get_log_level() const = 0;

    // Pure virtual raw logger to be implemented by specific backends
    virtual void log_raw(LogLevel level, std::string_view message) = 0;

    template<typename... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args) {
        log_raw(LogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        log_raw(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args) {
        log_raw(LogLevel::Warning, std::format(fmt, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        log_raw(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args) {
        log_raw(LogLevel::Fatal, std::format(fmt, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        log_raw(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
    }
};

} // namespace vynexos::core
