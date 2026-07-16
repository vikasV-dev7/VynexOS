<!-- AUTO-GENERATED START -->
# [[IAiRuntime]]

## Purpose
Interface for [[IAiRuntime]].

## Namespace
`vynexos::ai`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/ai/ai_runtime.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IAiRuntime]]() = default`
- `virtual std::expected<void, [[AiError]]> load_model(std::string_view model_name) = 0`
- `virtual void unload_model(std::string_view model_name) = 0`
- `virtual std::expected<std::unique_ptr<[[IAiSession]]>, [[AiError]]> create_session(std::string_view model_name) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
