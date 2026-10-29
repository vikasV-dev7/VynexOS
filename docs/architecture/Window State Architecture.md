# Window State Architecture

## Overview
Window state management in VynexOS dictates the lifecycle and presentation behavior of windows across the desktop environment. State is persistently owned by the `BasicWindowManager`, ensuring strict separation of concerns from the interaction owner (`DesktopShell`).

## Minimization Philosophy
In VynexOS, window minimization is treated strictly as a **presentation-state transition**, not an application lifecycle event.
- Minimized windows are **excluded** from the scene graph rendering pipeline (`BasicWindowManager::build_scene`).
- Minimized windows are **excluded** from input routing and hit-testing (`BasicWindowManager::get_window_at`, `BasicWindowManager::get_decoration_hit_region`).
- The `RunningApplication` lifecycle continues uninterrupted; the application remains resident in memory and continues to receive background updates and non-UI events.

## Components & Data Structures
- **`WindowState`**: A global enum defining presentation states: `Normal` and `Minimized`.
- **`WindowRecord`**: The internal metadata structure within `BasicWindowManager` that houses a window's persistent `WindowState` along with dimensions, z-order, and visibility flags.

## Interaction Flow
1. A user clicks the `MinimizeButton` decoration.
2. `DesktopShell::on_mouse_event` routes the click and resolves a `DecorationHitRegion::MinimizeButton`.
3. `DesktopShell` commands `BasicWindowManager::set_window_state(wid, WindowState::Minimized)`.
4. If the minimized window was currently focused, `DesktopShell` transfers focus to the next eligible window via `BasicWindowManager::get_visible_windows_z_ordered()`.
5. On the next `update_frame()`, `BasicWindowManager::build_scene()` completely omits the minimized window from the `SceneGraph`.
