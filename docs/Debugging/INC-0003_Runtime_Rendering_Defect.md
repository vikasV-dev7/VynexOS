# INC-0003: Runtime Rendering Defect Root Cause Analysis

## Observed Behaviour
- Clicking applications causes incorrect behaviour.
- Windows sometimes disappear or become hidden.
- The launcher does not behave correctly.
- UI appears to stop updating correctly after interaction.

## Pipeline Trace & Discrepancy Discovery

### 1. Input Processing & Focus Routing
`SDL2InputDriver` translates `SDL_MOUSEBUTTONDOWN`, `SDL_MOUSEBUTTONUP`, and `SDL_MOUSEMOTION` into `HAL_INPUT_MOUSE` events. The payload contains the **current** button state (e.g. 1 if left button is down).

**Discrepancy found in `BasicWindowManager::handle_mouse_event` & `DesktopShell::handle_mouse_event`**:
Both components check `if (mouse_ev.button_state != 0)` to detect a "click". However, because `SDL_MOUSEMOTION` includes the button state, dragging the mouse while holding the button triggers this check on *every single frame/motion event*. 
- In `BasicWindowManager`, this causes any window the mouse sweeps over to instantly jump to the front of the Z-order, erroneously stealing focus and covering (hiding) the active window.
- In `DesktopShell`, dragging over the Vynex button causes `m_launcher_visible` to toggle wildly (on/off every frame), breaking Launcher behaviour.

### 2. Surface Rendering & Pixel Accumulation
`DesktopShell`, `BasicTerminal`, `BasicFileExplorer`, and `DemoApp` are responsible for updating their `ISurface` buffers in `update_frame()`. 

**Discrepancy found in Application `update_frame()` logic**:
Applications draw to their surfaces using `BasicWidgetToolkit` (e.g., `draw_panel`, `draw_rect`). `BasicWidgetToolkit` implements intra-window alpha blending. However, none of the applications **clear** their surfaces before drawing the new frame. 
- `draw_panel` uses a semi-transparent colour (`Color{45, 45, 45, 240}`). When drawn repeatedly over 60 frames onto an uncleared surface, it blends the old pixels into the background but never completely erases them.
- Notifications drawn onto `m_wallpaper_surface` are never erased when they expire.
- Because old pixels are never cleared, the UI appears to "stop updating correctly" after interaction because previous UI states (like old scroll positions, removed notifications, or hidden buttons) remain permanently etched into the surface.

## Formulated Fix (Zero-Architectural-Change)

1. **Fix Click Detection**: Introduce a `uint8_t m_last_mouse_state{0}` to `BasicWindowManager` and `DesktopShell`. Change the click detection to `bool clicked = (mouse_ev.button_state != 0 && m_last_mouse_state == 0);`. This ensures focus and UI toggles only trigger on the actual MouseDown event, not on drag.
2. **Fix Surface Accumulation**: In the `update_frame()` method of `DesktopShell`, `BasicTerminal`, `BasicFileExplorer`, and `DemoApp`, add a block at the beginning to clear the surface. Since `BasicWidgetToolkit` cannot clear to transparent (alpha = 0 is a no-op), we directly map the pixels and zero them out:
   ```cpp
   auto map = m_surface->map_pixels();
   std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
   m_surface->unmap_pixels();
   ```
   *(For the Wallpaper, we fill with the solid `30, 30, 30, 255` background colour).*

**Note**: I have tentatively applied these minimal fixes to the source code to verify them.

Requesting permission to proceed with these changes as the resolution for INC-0003.
