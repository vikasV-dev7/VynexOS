<!-- AUTO-GENERATED START -->
# [[SoftwareSurface]]

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
- `~[[SoftwareSurface]]() override = default;`
- `uint32_t width() const override;`
- `uint32_t height() const override;`
- `std::span<uint8_t> map_pixels() override;`
- `void unmap_pixels() override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.123467+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/desktop/software_surface.hpp`

<!-- AUTO-GENERATED END -->
