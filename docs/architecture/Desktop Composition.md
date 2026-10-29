# Desktop Composition

## Overview
Desktop Composition in VynexOS refers to how discrete graphical layers are synthesized into a single cohesive visual output without relying on redundant copies of pixels. With the introduction of the Taskbar in 0.8.0.7, the composition hierarchy has expanded.

## Scene Graph Hierarchy
The `BasicWindowManager::build_scene()` algorithm constructs a linear array of `RenderLayer` structures from back to front:

1. **Background Layer**: The Desktop Wallpaper.
2. **Application Layers (Z-Ordered)**: 
    - Visible and non-minimized application windows.
    - Application window decorations (Title bar, borders, buttons).

### Layer 3: Application Content
This is the `client_bounds` specified by the Window Manager. The Shell coordinates with the [[Decoration Layout Engine]] to ensure application contents perfectly nest within the boundaries established by the [[DecorationLayout]].

### Layer 2: Window Decorations
These are rendered by `BasicWindowDecorator`, following strict geometric coordinates computed by the [[Decoration Layout Engine]].

3. **Taskbar Layer**: The Taskbar subsystem renders here, overlaying all application windows globally.
4. **Overlay Layers**: Launchers, Notifications, and the Cursor.

## Rendering Abstraction
Components like the `BasicTaskbar` do not communicate directly with display drivers. Instead, they write to a dedicated `ISurface` allocated by the `BasicCompositor`. During `update_frame`, the `DesktopShell` commands the compositor to present the `RenderLayer` array, which internally maps these surfaces to the final hardware backbuffer.

## Related Topics
- [[Taskbar Rendering]]
- [[Taskbar Architecture]]
