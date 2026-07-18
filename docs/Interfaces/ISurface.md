<!-- AUTO-GENERATED START -->
# [[ISurface]]

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
- `virtual ~[[ISurface]]() = default;`
- `virtual uint32_t width() const = 0;`
- `virtual uint32_t height() const = 0;`
- `virtual std::span<uint8_t> map_pixels() = 0;`
- `virtual void unmap_pixels() = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:39.685375+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/desktop_types.hpp`

<!-- AUTO-GENERATED END -->
