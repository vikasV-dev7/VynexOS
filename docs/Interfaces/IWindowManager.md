<!-- AUTO-GENERATED START -->
# [[IWindowManager]]

## Purpose
Interface for [[IWindowManager]].

## Namespace
`vynexos::desktop`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/window_manager.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IWindowManager]]() = default`
- `virtual uint32_t create_window(std::string_view title, WindowGeometry geometry) = 0`
- `virtual void destroy_window(uint32_t window_id) = 0`
- `virtual void focus_window(uint32_t window_id) = 0`
- `virtual void move_window(uint32_t window_id, int32_t x, int32_t y) = 0`
- `virtual void resize_window(uint32_t window_id, uint32_t width, uint32_t height) = 0`
- `virtual std::vector<uint32_t> get_windows_z_ordered() const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
