<!-- AUTO-GENERATED START -->
# [[BasicWidgetToolkit]]

## Purpose & Responsibilities
_Not explicitly documented._

## Dependencies & Interaction with CompositionRoot
_Constructor injected dependencies (Refer to Source Code)._

## Failure recovery & Performance considerations
**Failure Recovery**: _Unspecified_
**Performance**: _Unspecified_

## Extension points
_No explicit extension points defined._

## Public API
- `void draw_rect(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) override;`
- `void draw_text(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, std::string_view text, Color color) override;`
- `void draw_button(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) override;`
- `void draw_panel(std::shared_ptr<[[ISurface]]> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T07:54:11.606436+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/desktop/basic_widget_toolkit.hpp`

<!-- AUTO-GENERATED END -->




