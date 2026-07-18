<!-- AUTO-GENERATED START -->
# [[IAiRuntime]]

## Why the interface exists & Architectural role
_Not explicitly documented._

## Lifetime & Ownership expectations
**Lifetime**: _Unspecified_
**Ownership**: _Unspecified_

## Threading guarantees & Failure behaviour
**Thread Safety**: _Unspecified_
**Failure Handling**: _Unspecified_

## Extension guidance
_No explicit extension points defined._

## Public API
- `virtual ~[[IAiRuntime]]() = default;`
- `virtual std::expected<void, [[AiError]]> load_model(std::string_view model_name) = 0;`
- `virtual void unload_model(std::string_view model_name) = 0;`
- `virtual std::expected<std::unique_ptr<[[IAiSession]]>, [[AiError]]> create_session(std::string_view model_name) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.141764+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/ai/ai_runtime.hpp`

<!-- AUTO-GENERATED END -->










