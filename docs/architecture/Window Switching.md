# Window Switching

## Overview
Window Switching in VynexOS 0.8.0.7 represents the ability of the user to transition focus between running applications, typically facilitated by the Taskbar subsystem.

## Mechanism
The `BasicTaskbar` implementation synchronizes a state model of all running windows, maintaining stable button locations ordered by creation time rather than Z-order. 

When a user clicks a button on the Taskbar:
1. The Taskbar translates the click's local coordinates into a Window ID.
2. The `DesktopShell` intercepts this ID.
3. The `DesktopShell` directs the `BasicWindowManager` to restore the window if it was previously minimized.
4. The `DesktopShell` commands the `BasicWindowManager` to focus the window.
5. The `DesktopShell` updates its internal runtime state to reflect the new focused application.

Because the taskbar uses stable creation-order enumeration (`get_window_snapshots()`), the buttons do not shift wildly when a new window gains focus, ensuring a predictable user experience for window switching.

## Related Topics
- [[Taskbar Architecture]]
- [[Window Activation]]
