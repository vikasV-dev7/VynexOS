<!-- AUTO-GENERATED START -->
# [[IDesktopShell]]

## Purpose
Interface for [[IDesktopShell]].

## Namespace
`vynexos::desktop`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/desktop_shell.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IDesktopShell]]() = default`
- `virtual void toggle_application_launcher() = 0`
- `virtual void update_taskbar() = 0`
- `virtual void lock_screen() = 0`
- `virtual std::vector<uint32_t> get_overlay_window_ids() const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
