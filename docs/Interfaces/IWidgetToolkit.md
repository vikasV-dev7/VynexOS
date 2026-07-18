<!-- AUTO-GENERATED START -->
# [[IWidgetToolkit]]

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
- `virtual ~[[IWidgetToolkit]]() = default;`
- `virtual void draw_rect(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) = 0;`
- `virtual void draw_text(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, std::string_view text, Color color) = 0;`
- `virtual void draw_button(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) = 0;`
- `virtual void draw_panel(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T07:54:11.306103+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/widget_toolkit.hpp`

<!-- AUTO-GENERATED END -->




