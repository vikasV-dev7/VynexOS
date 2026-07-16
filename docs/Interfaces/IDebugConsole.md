<!-- AUTO-GENERATED START -->
# [[IDebugConsole]]

## Purpose
Interface for [[IDebugConsole]].

## Namespace
`vynexos::sdk`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/sdk/debug_console.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IDebugConsole]]() = default`
- `virtual std::expected<std::string, std::string> execute_command(std::string_view cmd) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
