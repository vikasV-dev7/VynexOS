---
title: Version 0.6.0 Test Stabilization
date: 2026-07-18
tags: [debugging, rendering, widget-toolkit, desktop-shell]
---

# Test Stabilization Investigation

**Issue ID**: TST-060-01  
**Date**: 2026-07-18  
**Phase**: Version 0.6.0 Feature Completion & Release Stabilization  
**Component**: DesktopShell / WidgetToolkit / Unit Tests  
**Severity**: High (Build / Test Failure)  

---

## 1. Error Message
```
Assertion failed: task_map[0] == 45, file src/tests/test_desktop_shell.cpp, line 59
```

## 2. Build Output
The executable aborts because the `assert(task_map[0] == 45)` fails during CTest execution of `test_desktop_shell`.

## 3. Root Cause
The unit test expected the taskbar background pixel to be `45`. The value `45` was correct when `draw_panel` originally drew opaque pixels (or before intra-window alpha blending was fully implemented). 
However, in Phase 3, `BasicWidgetToolkit::draw_rect` was updated to support alpha blending, and `draw_panel` uses `Color{45, 45, 45, 240}`. 
When this semi-transparent color (alpha = 240) is drawn over an initialized `0` background (`std::fill` in `DesktopShell::launch`), the final pixel value is calculated as `45 * 240 / 255.0f`, which mathematically yields `42.35` and truncates to `42`.

The defect was in the **unit test**, not the implementation. The implementation correctly executed alpha blending over the unmapped, zeroed `SoftwareSurface`.

## 4. Investigation Process
1. Inspected the failing assertion in `src/tests/test_desktop_shell.cpp`.
2. Traced the taskbar drawing logic inside `DesktopShell::update_frame()`.
3. Verified that `DesktopShell` calls `m_toolkit->draw_panel(m_taskbar_surface, 0, 0, 1920, 40)`.
4. Examined `BasicWidgetToolkit::draw_panel()`, which passes `Color{45, 45, 45, 240}` to `draw_rect`.
5. Analyzed `BasicWidgetToolkit::draw_rect()`, observing the `alpha` blending algorithm (`(color.r * alpha) + (dr * inv_alpha)`).
6. Simulated the exact math: `(45 * (240 / 255.0f)) + (0 * (15 / 255.0f)) = 42.3529`. Truncated to `uint8_t`, the expected output is `42`.
7. Concluded the implementation correctly fulfills the Phase 3 alpha blending requirement, but the test expectations were stale.

## 5. Resolution
Updated `test_desktop_shell.cpp` and `test_widget_toolkit.cpp` to assert the correct alpha-blended value (`42`) and verify all RGBA channels properly.

## 6. Files Modified
- `src/tests/test_desktop_shell.cpp`
- `src/tests/test_widget_toolkit.cpp`

## 7. Architecture Impact
None. This was strictly a test expectation mismatch.

## 8. Why the Bug Occurred
When Phase 3 implemented intra-window alpha blending, existing opaque rendering assertions were not systematically updated to reflect their new alpha-blended mathematical limits over black backgrounds.

## 9. Preventive Measures
- Tests explicitly asserting pixel colors must be reviewed when blending modes or opacity tokens change.
- Comment expected mathematical operations next to pixel color assertions (e.g. `// Result = 45 * 240 / 255 = 42`).

## 10. Lessons Learned
Legacy tests acting as strict regression gates will reliably fail if their expected values aren't parameterized or audited when the underlying rendering formulas change.

## 11. Related Components
- `BasicWidgetToolkit`
- `DesktopShell`
- `SoftwareSurface`

## 12. Future Risks
If global opacity or gamma correction is introduced, these static assertions will fail again. Future implementations might need tolerance-based checks rather than exact integer matching.

---
