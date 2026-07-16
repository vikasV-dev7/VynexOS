<!-- AUTO-GENERATED START -->
# [[IConfigManager]]

## Purpose
Interface for [[IConfigManager]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/config_manager.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IConfigManager]]() = default`
- `virtual std::expected<std::any, [[ConfigError]]> get_value(std::string_view key) const = 0`
- `virtual std::expected<void, [[ConfigError]]> set_value(std::string_view key, std::any value) = 0`
- `virtual bool has_key(std::string_view key) const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
