# Window Activation

## Overview
Window Activation defines the exact workflow sequence invoked when a window becomes the active input target in VynexOS. In version 0.8.0.7, window activation routes deterministically through the `DesktopShell`, which enforces the state modifications in the `BasicWindowManager`.

## The Activation Workflow
When a taskbar entry is clicked or a window's body is clicked, the following sequence occurs:

1. **State Restoration**: If the window is currently Minimized, its state is transitionally flipped to Normal via `set_window_state(Normal)`.
2. **Raise (Z-Order)**: The window is promoted to the top of the `m_z_order` stack inside `BasicWindowManager`.
3. **Focus Acquisition**: The `focus_window()` API is invoked, updating `m_focused_window` in the manager.
4. **Shell Routing**: `DesktopShell` registers the new active application in its internal tracked `m_focused_app` pointer for rapid dispatching of keyboard events.
5. **Taskbar Synchronization**: `DesktopShell::update_frame` fetches the latest snapshots via `get_window_snapshots()` and alerts the Taskbar about the new focus state.
6. **Re-render**: The taskbar's `draw()` loop applies the active visual style (e.g. blue highlighting) to the newly activated application's button.

## Related Topics
- [[Window Switching]]
- [[Taskbar Architecture]]
