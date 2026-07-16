<!-- AUTO-GENERATED START -->
# [[IPlugin]]

## Purpose
Interface for [[IPlugin]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/plugin_manager.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IPlugin]]() = default`
- `virtual std::string_view get_name() const = 0`
- `virtual std::string_view get_description() const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
