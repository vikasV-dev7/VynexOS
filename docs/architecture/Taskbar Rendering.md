# Taskbar Rendering

## Overview
The `BasicTaskbar` operates as an independent presentation layer within the Desktop runtime. Its single responsibility is parsing the synchronized `TaskbarEntry` model and projecting it onto an `ISurface`.

## Rendering Pipeline
1. **Clear Surface**: The base panel is typically drawn by the `DesktopShell` via `IWidgetToolkit::draw_panel()` prior to delegating to `BasicTaskbar`.
2. **Synchronize State**: `BasicTaskbar::synchronize(entries)` caches the latest known snapshots of running windows.
3. **Draw Buttons**: `BasicTaskbar::draw()` iterates through the cached entries.
    - It computes horizontal offsets using configured margins (e.g., `kStartX`, `kButtonWidth`).
    - It selects a target color depending on the entry's `state` (Normal vs Minimized) and `is_active` flag.
    - Active windows are rendered with highlighted backgrounds.
    - Minimized windows are rendered with dimmed backgrounds.
4. **Text Rendering**: Truncated window titles are rendered centrally inside the bounds of the taskbar button using `IWidgetToolkit::draw_text()`.

## Threading Guarantees
Taskbar rendering operates synchronously on the UI thread as part of `update_frame`. It holds an internal mutex during drawing and hit-testing to prevent data races against `synchronize()` if synchronization were ever driven asynchronously. Currently, all desktop components abide by deterministic frame-loop execution without additional synchronization primitives blocking the scheduler.

## Related Topics
- [[Taskbar Architecture]]
- [[Desktop Composition]]
