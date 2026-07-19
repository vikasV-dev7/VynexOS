# Version 0.6.0 Architecture Principles
- **Lifecycle Ownership**: CompositionRoot is the absolute owner of the runtime lifecycle. It will directly query the HAL to determine shutdown state, breaking its internal run-loop when a shutdown is requested.
- **EventBus Purity**: Application messaging infrastructure (the EventBus) will not be used to transport lifecycle termination events, preserving clean separation of concerns.
- **Rendering Determinism**: The Compositor must operate strictly based on mathematical alpha-blending logic (`ISurface`) without requiring dynamic memory allocations during steady-state rendering.
- **Input Determinism**: The `IInputDriver` abstraction must guarantee ordered input event states natively, completely isolating core OS logic from asynchronous physical driver behavior (e.g., eliminating `SDL_GetMouseState` race conditions).

## Final Verification Checklist
- [x] SOLID principles remain intact.
- [x] Dependency Injection remains unchanged.
- [x] CompositionRoot remains the lifecycle owner.
- [x] EventBus responsibilities remain unchanged (transporting application/domain events).
- [x] No unnecessary subsystem coupling has been introduced.
- [x] No dynamic allocations occur inside the core compositor frame loop.
