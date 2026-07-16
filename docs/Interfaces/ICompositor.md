<!-- AUTO-GENERATED START -->
# [[ICompositor]]

## Purpose
Interface for [[ICompositor]].

## Namespace
`vynexos::desktop`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/compositor.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[ICompositor]]() = default`
- `virtual void submit_buffer(const WindowBuffer& buffer) = 0`
- `virtual std::expected<void, [[DisplayError]]> render_frame() = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
