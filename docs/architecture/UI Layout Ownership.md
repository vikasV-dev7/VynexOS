# UI Layout Ownership

VynexOS adheres to strict separation of concerns regarding the windowing ecosystem:

- **BasicWindowManager:** Owns Canonical Window Geometry, Canonical Decorated Bounds, and constructs [[Window Frame Metrics]]. Consumes layout for hit testing.
- **DesktopShell:** Orchestrates runtime flow, requests decoration layout generation, and proxies the layout down to the decorator.
- **BasicWindowDecorator:** Strictly consumes layout for rendering. Performs absolutely no geometry math.
- **BasicCompositor:** Owns generic surface rendering. Blind to window concepts entirely.
- **Decoration Layout Engine:** Generates pure layout data structure representing bounds. Is completely decoupled from logic such as `point_in_rect()` hit-testing validations or color assignments.
