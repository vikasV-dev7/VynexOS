# Technical Debt Tracker

This document formally tracks deferred architecture decisions, workarounds, and optimizations within VynexOS. Every deferred item includes context, risk assessment, and a planned resolution path.

## TD-0001: Window configuration is currently hardcoded.

*   **ID**: TD-0001
*   **Description**: The display dimensions (1024x768) and initialization flags in `[[SDL2DisplayBackend]]` are currently hardcoded directly into the C++ source files.
*   **Why deferred**: A proper resolution requires a unified configuration system (manifests/settings) capable of runtime display settings, which does not exist in the current architecture layer.
*   **Architectural impact**: Forces rebuilds to change initial resolution and limits flexibility across different target deployment environments.
*   **Risk**: Low
*   **Planned milestone**: v0.6.x
*   **Resolution status**: Unresolved. Deferred until the Configuration/Settings subsystem is formalized.

## TD-0002: Static Mock UI in File Explorer
*   **ID**: TD-0002
*   **Description**: The `BasicFileExplorer` renders static, non-interactive "Tabs" and empty file mock data.
*   **Why deferred**: The current architecture strictly enforces mock-services. Adding real interactivity here requires a functional Virtual File System (VFS) to back it up, which does not exist in v0.6.0.
*   **Architectural impact**: Minimal for the compositor, but restricts true Desktop Application functionality.
*   **Risk**: Low
*   **Planned milestone**: v0.7.0 (VFS Integration)
*   **Resolution status**: Unresolved.

## TD-0003: Limited Font Rendering Engine
*   **ID**: TD-0003
*   **Description**: `BasicWidgetToolkit` relies on a highly constrained `font8x8` 1-bit monochrome array.
*   **Why deferred**: Implementing FreeType/TrueType parsing requires dynamic memory buffers and complex glyph caching which conflicts with the Phase 2 zero-allocation constraints.
*   **Architectural impact**: Text rendering cannot be resized, bolded, or utilize modern anti-aliasing.
*   **Risk**: Medium (Usability degradation)
*   **Planned milestone**: v0.8.0
*   **Resolution status**: Unresolved.

---

# Resolved Technical Debt

## RTD-0001: Excessive Dynamic Allocations in BasicCompositor
*   **Description**: Pre-v0.6.0, `BasicCompositor` dynamically allocated an 8MB `std::vector` inside every single `render()` frame.
*   **Resolution**: Resolved in v0.6.0 by elevating the buffer to a class-scoped member `m_master_fb`, achieving 0 allocations per frame.

## RTD-0002: MSVC Debug Bounds Checking Overhead
*   **Description**: `std::vector::operator[]` inside nested 1920x1080 alpha blending loops caused 5.8 FPS stuttering.
*   **Resolution**: Resolved in v0.6.0 by utilizing raw pointer arithmetic (`data()`) and `std::memcpy` optimizations, restoring framerates by 338% without abandoning memory safety.
