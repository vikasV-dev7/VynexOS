<!-- AUTO-GENERATED START -->
# [[IDesktopShell]]

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
- `virtual ~[[IDesktopShell]]() = default;`
- `virtual void toggle_application_launcher() = 0;`
- `virtual void update_taskbar() = 0;`
- `virtual void lock_screen() = 0;`
- `virtual std::vector<uint32_t> get_overlay_window_ids() const = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:39.676743+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/desktop_shell.hpp`

<!-- AUTO-GENERATED END -->




