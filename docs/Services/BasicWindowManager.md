<!-- AUTO-GENERATED START -->
# [[BasicWindowManager]]

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
- `~[[BasicWindowManager]]() override = default;`
- `uint32_t create_window(std::string_view title, WindowGeometry geometry) override;`
- `void destroy_window(uint32_t window_id) override;`
- `void focus_window(uint32_t window_id) override;`
- `void move_window(uint32_t window_id, int32_t x, int32_t y) override;`
- `void resize_window(uint32_t window_id, uint32_t width, uint32_t height) override;`
- `void set_window_surface(uint32_t window_id, std::shared_ptr<[[ISurface]]> surface) override;`
- `void set_window_visibility(uint32_t window_id, bool visible) override;`
- `std::vector<uint32_t> get_windows_z_ordered() const override;`
- `SceneGraph build_scene() const override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.585238+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/desktop/basic_window_manager.hpp`

<!-- AUTO-GENERATED END -->





