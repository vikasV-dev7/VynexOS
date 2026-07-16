#pragma once
#include <string_view>
#include <expected>
#include <memory>
#include <functional>
#include "vynexos/ai/ai_runtime.hpp"

namespace vynexos::apps {

class ITerminal {
public:
    virtual ~ITerminal() = default;
    
    // Core terminal I/O
    virtual void write_output(std::string_view text) = 0;
    virtual void on_input(std::function<void(std::string_view)> callback) = 0;
    
    // AI Integration:
    // Asks the local AI to explain a command or error
    virtual void request_ai_explanation(std::string_view command_context) = 0;
    
    // Auto-suggests commands based on semantic intent
    virtual void request_ai_suggestion(std::string_view intent) = 0;
};

} // namespace vynexos::apps
