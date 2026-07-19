# Version 0.6.0 Debugging History

This document serves as the institutional engineering debugging record for Version 0.6.0. It catalogs the major incidents, investigations, root causes, and implemented fixes during the stabilization of the Runtime, Input, and Rendering pipelines.

---

## 1. Disappearing Windows & Duplicate Mouse Clicks
- **Problem**: Desktop application windows (such as the Launcher) would spontaneously disappear when clicked, and UI focus would thrash randomly.
- **Symptoms**: Clicking the "Start" button would spawn a launcher window that would instantly vanish on the next frame.
- **Alternative Hypotheses**: 
  - The `BasicWindowManager` `std::list` iterators were being invalidated during multi-threaded modification.
  - Z-ordering logic was sorting incorrectly.
- **Evidence Collected**: Extensive logs showed that the `m_launcher_visible` boolean was toggling rapidly multiple times per physical click. 
- **Final Root Cause**: Non-deterministic input generation. The `SDL2InputDriver` was calling `SDL_GetMouseState()` directly inside the event processing loop. Because SDL updates its state asynchronously, `GetMouseState` returned the same "Button Down" state across multiple ticks, causing VynexOS to emit multiple distinct "Click" events for a single physical button press.
- **Implemented Fix**: Removed the dependency on `SDL_GetMouseState`. Introduced an internal `m_mouse_button_state` array inside `SDL2InputDriver` to track the exact sequence of `SDL_MOUSEBUTTONDOWN` and `SDL_MOUSEBUTTONUP` events, yielding 100% deterministic edge-triggered clicks.
- **Regression Test Added**: `test_sdl_integration.cpp` burst input tests, verifying down/up state constraints.
- **Lessons Learned**: Never rely on asynchronous subsystem state-query functions inside a deterministic event processing loop. Queue state must be tracked internally.

---

## 2. Alpha Blending Artifacts (Ghosting)
- **Problem**: Fully opaque background windows were bleeding through foreground windows when dragging over them.
- **Symptoms**: Drawing a 255-alpha window over a white background produced a muddy, semi-transparent ghost image instead of a solid color block.
- **Root Cause Investigation**: Analyzed the mathematical equation inside `BasicCompositor::BlendPixel`.
- **Final Root Cause**: The pixel data provided by `BasicWidgetToolkit` was already pre-multiplied by alpha. The Compositor was multiplying it by alpha *again*, mathematically reducing the RGB intensity and breaking the `1.0 - alpha` background calculation.
- **Implemented Fix**: Fixed `SoftwareSurface` alpha blending algorithm. Stripped out the double pre-multiplication. Enforced strict 0-255 scaling logic natively.

---

## 3. MSVC Debug Iterator Performance Bottleneck
- **Problem**: Phase 3 performance tests revealed severe runtime stuttering, running at 5.8 FPS despite 0 memory allocations.
- **Symptoms**: `vynex_init.exe` consumed 100% of a CPU core, and average frame render times were ~171.1ms.
- **Root Cause Investigation**: Profiling the `ICompositor::render()` pipeline isolated the bottleneck specifically to the `mapped[src_idx]` pixel indexing logic in `BasicCompositor.cpp`.
- **Final Root Cause**: When compiling with MSVC in `x64-Debug`, the C++ standard library `std::vector::operator[]` forces extensive debug bounds-checking and iterator invalidation safety assertions. Running this inside a `1920x1080` double-nested loop resulted in over 2 million debug function calls per frame.
- **Implemented Fix**: Bypassed debug iterators inside the performance-critical inner loop using raw pointer arithmetic (`uint32_t* src_ptr = mapped.data()`). Implemented `std::memcpy` for fully opaque rows to skip pixel-by-pixel calculations entirely.
- **Lessons Learned**: Safe, modern C++ (like `std::vector` indexing) carries catastrophic overhead in Debug configurations if used inside extreme high-frequency inner loops. Strategic fallback to raw pointer manipulation is necessary and acceptable inside constrained performance boundaries.

---

## 4. Architecture Consistency Review (INC-0004)
- **Problem**: Regression test `test_runtime_bug.cpp` refused to compile, stating `vynexos::core::InMemoryEventBus` did not exist.
- **Symptoms**: The Phase 3 test suite contained fabricated classes that violated the established mock-services architecture.
- **Root Cause Investigation**: A test author attempted to instantiate a fake event bus directly and inject it into the `CompositionRoot` to observe internal signals, violating the strict `CompositionRoot` encapsulation rules.
- **Implemented Fix**: Enforced the Architecture Consistency Review. Replaced `InMemoryEventBus` with the official `DummyEventBus`. Modified the test to use standard public interfaces by fetching the `IInputDriver` and dynamically casting it to observe state.
- **Lessons Learned**: Do not modify production interfaces or invent production classes solely to satisfy tests. Tests must validate the existing architecture rather than redefine it.
