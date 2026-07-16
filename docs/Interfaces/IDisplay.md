<!-- AUTO-GENERATED START -->
# [[IDisplay]]

## Purpose
Interface for [[IDisplay]].

## Namespace
`vynexos::hal`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/hal/display.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IDisplay]]() = default`
- `[[nodiscard]] virtual std::expected<void, [[DisplayError]]> initialize() = 0`
- `[[nodiscard]] virtual std::expected<void, [[DisplayError]]> present(const FrameBuffer& buffer) = 0`
- `virtual void shutdown() = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
