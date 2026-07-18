# Technical Debt Tracker

This document formally tracks deferred architecture decisions, workarounds, and optimizations within VynexOS. Every deferred item includes context, risk assessment, and a planned resolution path.

## TD-0001: Window configuration is currently hardcoded.

*   **ID**: TD-0001
*   **Description**: The display dimensions (1024x768) and initialization flags in `SDL2DisplayBackend` are currently hardcoded directly into the C++ source files.
*   **Why deferred**: A proper resolution requires a unified configuration system (manifests/settings) capable of runtime display settings, which does not exist in the current architecture layer.
*   **Architectural impact**: Forces rebuilds to change initial resolution and limits flexibility across different target deployment environments.
*   **Risk**: Low
*   **Planned milestone**: v0.6.x
*   **Resolution status**: Unresolved. Deferred until the Configuration/Settings subsystem is formalized.
