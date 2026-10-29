# Window Decorations

Window decorations (title bars, borders, and buttons) in VynexOS are governed by strict [[UI Layout Ownership]] principles.

## Rendering
Decorations are visually rendered by the `BasicWindowDecorator`, utilizing a `BasicWidgetToolkit`. 

## Geometry Generation
Because decoration math is prone to fragmentation between rendering and hit-testing (as observed in VynexOS prior to Version 0.8.0.9), decorations are now dictated exclusively by the [[Decoration Layout Engine]]. 

- The Window Manager establishes the [[Window Frame Metrics]] based on the canonical window bounds.
- The Engine maps these metrics into a purely declarative [[DecorationLayout]].
- The renderer consumes this layout.
- The window manager consumes this layout for interaction routing.

This [[Shared Decoration Geometry]] architecture means that decorations can be resized (e.g. for accessibility, DPI scaling) without risking interactive desynchronization.
