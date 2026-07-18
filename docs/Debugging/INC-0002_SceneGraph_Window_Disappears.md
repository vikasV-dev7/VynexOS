# INC-0002: SceneGraph Discrepancy & Window Disappearance

## Issue Summary
During boot, the WindowManager creates 6 windows. However, instrumentation showed the SceneGraph consistently contained only 5 RenderLayers. Furthermore, clicking on applications caused them to visually disappear or become "almost invisible," and the desktop appeared to visually freeze despite the render loop continuing to run perfectly.

## Observed Behaviour
* `WindowManager` registers 6 windows: Wallpaper, Taskbar, Launcher, Terminal, File Explorer, Demo App.
* `SceneGraph` generation only outputs 5 `RenderLayer` objects.
* Frame loop continues without deadlock or blocking.
* Clicking windows corrupts the display or makes them disappear, but the process does not crash.

## Expected Behaviour
* All created and visible windows should be rendered.
* Clicking a window should route focus to it and bring it to the front of the Z-order without hiding other windows.
* The desktop background should remain at the back of the Z-order.

## Instrumentation Added
Instrumentation was injected into:
1. `BasicWindowManager::build_scene()`: To print registered windows, their states (opacity, visibility, bounds), and the resulting generated `SceneGraph` layers.
2. `BasicCompositor::render_scene_internal()`: To print every layer being blended, including its bounds, clip, opacity, and surface pointers.

## SceneGraph Comparison & Root Cause

### 1. The Missing Window (SceneSize = 5)
By comparing the Registered Windows logs vs the SceneGraph logs, the missing window was definitively identified as the **Launcher**.
**Reason for exclusion:** The `Launcher` is instantiated with `visible = false` by `DesktopShell` because it acts as a popup menu that only appears when the Vynex button on the Taskbar is clicked. `BasicWindowManager::build_scene()` correctly contains the logic `if (!win.visible) continue;`, entirely excluding it from the SceneGraph generation.

### 2. The Window Disappearance (Z-Order Corruption)
The log instrumentation revealed that `BasicWindowManager::handle_mouse_event` failed to filter windows based on `visibility` and `title`. 
When clicking the screen:
1. Clicking the desktop background matched the bounds of the `Wallpaper` window (`1920x1080`). `handle_mouse_event` routed focus to the `Wallpaper`, erasing it from the Z-order and pushing it to the back of the vector (the top). The Compositor rendered the opaque `Wallpaper` last, covering all applications and creating the illusion of a freeze.
2. Clicking the bottom-left area of the screen matched the bounds of the invisible `Launcher`. `handle_mouse_event` routed focus to the `Launcher`. The application underneath failed to receive focus or come to the front.

## Files Modified
* `include/vynexos/desktop/desktop_types.hpp` - Added `window_id` to `RenderLayer` for compositor tracking.
* `src/desktop/basic_window_manager.cpp` - Patched `handle_mouse_event` to explicitly ignore `!win.visible` and `win.title == "Wallpaper"`. Refactored Z-order erasure using `std::find`.

## Fix & Validation
The Window Manager mouse event handler was modified to explicitly skip invisible windows and the desktop background. The build was verified and runtime testing confirmed that clicking no longer corrupts the Z-order, and background elements remain permanently at the bottom.

## Lessons Learned & Future Prevention
* **System vs Application Surfaces:** The Window Manager currently treats all surfaces identically. In future architecture revisions (post v0.6.0), explicit roles (e.g., `WindowRole::DesktopBackground`, `WindowRole::Overlay`) should be introduced to dictate focus and Z-order rules, rather than relying on string matching (`win.title == "Wallpaper"`).
* **Visibility States:** UI interaction layers MUST share the exact same state evaluation as the rendering pipeline. A window that is excluded from the SceneGraph due to `!visible` must strictly be excluded from hit-testing.
