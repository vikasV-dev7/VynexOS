<!-- AUTO-GENERATED START -->
# [[IConfigManager]]

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
- `virtual ~[[IConfigManager]]() = default;`
- `virtual std::expected<std::any, [[ConfigError]]> get_value(std::string_view key) const = 0;`
- `virtual std::expected<void, [[ConfigError]]> set_value(std::string_view key, std::any value) = 0;`
- `virtual bool has_key(std::string_view key) const = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.156348+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/config_manager.hpp`

<!-- AUTO-GENERATED END -->










