<!-- AUTO-GENERATED START -->
# [[IWindowManager]]

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
- `virtual ~[[IWindowManager]]() = default;`
- `virtual uint32_t create_window(std::string_view title, WindowGeometry geometry) = 0;`
- `virtual void destroy_window(uint32_t window_id) = 0;`
- `virtual void focus_window(uint32_t window_id) = 0;`
- `virtual void move_window(uint32_t window_id, int32_t x, int32_t y) = 0;`
- `virtual void resize_window(uint32_t window_id, uint32_t width, uint32_t height) = 0;`
- `virtual void set_window_surface(uint32_t window_id, std::shared_ptr<[[ISurface]]> surface) = 0;`
- `virtual void set_window_visibility(uint32_t window_id, bool visible) = 0;`
- `virtual std::vector<uint32_t> get_windows_z_ordered() const = 0;`
- `virtual SceneGraph build_scene() const = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.287209+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/window_manager.hpp`

<!-- AUTO-GENERATED END -->





