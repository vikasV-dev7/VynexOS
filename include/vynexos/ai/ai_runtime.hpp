#pragma once
#include <string_view>
#include <string>
#include <expected>
#include <cstdint>
#include <memory>
#include <functional>

namespace vynexos::ai {

enum class AiError {
    ModelNotFound,
    OutOfMemory,
    BackendFailure,
    Timeout
};

struct InferenceResult {
    std::string text_output;
    double confidence_score;
    uint32_t tokens_generated;
};

class IAiSession {
public:
    virtual ~IAiSession() = default;
    
    using AsyncCallback = std::function<void(std::expected<InferenceResult, AiError>)>;

    // Non-blocking asynchronous inference (Graceful Degradation compliant)
    virtual void prompt_async(std::string_view prompt, AsyncCallback callback) = 0;
};

class IAiRuntime {
public:
    virtual ~IAiRuntime() = default;
    
    // Model Lifecycle Management
    virtual std::expected<void, AiError> load_model(std::string_view model_name) = 0;
    virtual void unload_model(std::string_view model_name) = 0;
    
    // Session lifecycle isolated per client application
    virtual std::expected<std::unique_ptr<IAiSession>, AiError> create_session(std::string_view model_name) = 0;
};

} // namespace vynexos::ai
