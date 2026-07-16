<!-- AUTO-GENERATED START -->
# [[IComputeDevice]]

## Purpose
Interface for [[IComputeDevice]].

## Namespace
`vynexos::hal`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/hal/compute.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IComputeDevice]]() = default`
- `[[nodiscard]] virtual std::expected<void, [[ComputeError]]> initialize() = 0`
- `[[nodiscard]] virtual std::expected<void, [[ComputeError]]> execute_kernel(std::span<const uint8_t> shader_code) = 0`
- `virtual void shutdown() = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
