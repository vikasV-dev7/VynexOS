#pragma once
#include <string_view>
#include <expected>
#include <string>

namespace vynexos::sdk {

class IDebugConsole {
public:
    virtual ~IDebugConsole() = default;
    
    // Execute interactive developer commands (e.g., trace IPC, mock events)
    virtual std::expected<std::string, std::string> execute_command(std::string_view cmd) = 0;
};

} // namespace vynexos::sdk
