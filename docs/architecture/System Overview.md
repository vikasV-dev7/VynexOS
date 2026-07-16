<!-- AUTO-GENERATED START -->
# System Overview

## Architecture Overview
```mermaid
graph TD title System Architecture
    Core[vynex_core]
    HAL[vynex_hal]
    Desktop[vynex_desktop]
    Apps[vynex_apps]
    Init[vynex_init]
    Init --> Apps
    Apps --> Desktop
    Desktop --> Core
    HAL --> Core
    Init --> HAL
    Init --> Desktop
    Init --> Core
```

## Subsystems
- [[Runtime]]
- [[HAL]]
- [[Desktop Shell]]
<!-- AUTO-GENERATED END -->
