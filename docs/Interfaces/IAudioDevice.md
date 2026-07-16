<!-- AUTO-GENERATED START -->
# [[IAudioDevice]]

## Purpose
Interface for [[IAudioDevice]].

## Namespace
`vynexos::hal`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/hal/audio.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IAudioDevice]]() = default`
- `[[nodiscard]] virtual std::expected<void, [[AudioError]]> initialize() = 0`
- `[[nodiscard]] virtual std::expected<void, [[AudioError]]> play(std::span<const int16_t> pcm_data) = 0`
- `virtual void shutdown() = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
