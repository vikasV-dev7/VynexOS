<!-- AUTO-GENERATED START -->
# [[IWidgetToolkit]]

## Purpose
Interface for [[IWidgetToolkit]].

## Namespace
`vynexos::desktop`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/widget_toolkit.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IWidgetToolkit]]() = default`
- `virtual void draw_rect(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) = 0`
- `virtual void draw_text(WindowBuffer& buffer, int32_t x, int32_t y, std::string_view text, Color color) = 0`
- `virtual void draw_button(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) = 0`
- `virtual void draw_panel(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
