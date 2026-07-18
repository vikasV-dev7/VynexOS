# INC-0001: Runtime Defect - Window Disappears on Click / Desktop Freeze

## 1. Symptoms
During final release stabilization for Version 0.6.0, an integration test run revealed a critical runtime defect:
* VynexOS boots successfully and the desktop shell (Wallpaper, Taskbar, Launcher) and applications (Terminal, File Explorer, Demo App) appear correctly.
* Clicking on an application causes it to disappear or become visually corrupted.
* Shortly afterwards, the desktop appears to freeze or rendering stops updating entirely.

## 2. Root Cause Analysis
An extensive runtime investigation into `BasicWindowManager` and `BasicCompositor` revealed the following interacting causes:

### 2.1 Background Window Focus Stealing (The "Freeze")
`BasicWindowManager::handle_mouse_event` is responsible for routing mouse clicks and bringing the clicked window to the top of the Z-order. It does this by iterating the Z-order backwards (top-to-bottom) and checking if the mouse coordinates fall within the `WindowGeometry` of each window.

The `DesktopShell` creates the desktop background as a standard window named `"Wallpaper"` with a geometry of `0, 0, 1920, 1080`. 
Because `handle_mouse_event` did not differentiate between standard applications and background system surfaces, clicking on an empty area of the desktop background caused the Window Manager to route focus to the `"Wallpaper"` window. 

When focused, the `"Wallpaper"` window was moved from the bottom of the Z-order to the top. During rendering, `BasicCompositor::render_scene_internal` drew the `Wallpaper` last. Because the `Wallpaper` is an opaque `30, 30, 30, 255` surface spanning the entire screen, it was drawn over the top of **all** desktop applications and the Taskbar. 

By hiding all applications and the Taskbar (which contains the updating system clock), the system gave the user the strong illusion that the desktop had completely frozen, when in reality, it was rendering correctly, just with the background drawn over everything.

### 2.2 Invisible Window Focus Stealing (The "Disappearance")
The `DesktopShell` also creates a `"Launcher"` window at `10, 640` sized `300x400`. When closed, its visibility is set to `false`. 
However, `handle_mouse_event` failed to check the `win.visible` property when evaluating bounds. 

If a user dragged a window (like `File Explorer`) over the bottom-left corner of the screen and clicked it, the Window Manager would detect a bounds match on the invisible `"Launcher"` window (since it is higher in the original Z-order) rather than the application. It would route focus to the invisible `Launcher`, bringing it to the top. The application underneath would fail to come to the front, breaking the expected Z-order mechanics and causing the UI to feel "broken" or visually incorrect.

### 2.3 Iterator Erase Safety
While investigating, the original Z-order reordering logic used `m_z_order.erase(std::next(it).base());` on a reverse iterator. While mathematically valid, this pattern is highly brittle and heavily frowned upon in modern C++ codebases.

## 3. Resolution
The defect was resolved entirely within `BasicWindowManager::handle_mouse_event` by applying proper constraints to focus routing. The frozen Version 0.6.0 architecture was preserved.

**Modifications:**
1. **Visibility Check:** Added a strict `!win.visible` check to skip invisible windows. The closed Launcher can no longer steal focus.
2. **Background Check:** Added a strict `win.title == "Wallpaper"` check to prevent the desktop background from ever being routed focus and brought to the front.
3. **Iterator Safety:** Refactored the Z-order focus update to use a forward `std::find(m_z_order.begin(), m_z_order.end(), win_id)` to locate and erase the target window. This makes the logic undeniably correct and easy to audit.

No APIs were modified, and the `ISurface` rendering pipeline continues to function identically to the architecture specification.
