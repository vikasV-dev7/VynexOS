<!-- AUTO-GENERATED START -->
# [[ICompositor]]

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
- `virtual ~[[ICompositor]]() = default;`
- `virtual std::shared_ptr<[[ISurface]]> create_surface(uint32_t width, uint32_t height) = 0;`
- `virtual std::expected<void, [[DisplayError]]> render_frame(const SceneGraph& scene) = 0;`
- `virtual bool render_frame(const SceneGraph& scene) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T07:54:11.265989+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/compositor.hpp`

<!-- AUTO-GENERATED END -->




