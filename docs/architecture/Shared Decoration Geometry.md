# Shared Decoration Geometry

Prior implementations suffered from coordinate drift because decorators and hit testers maintained independent mathematical models of UI metrics. VynexOS enforces Shared Decoration Geometry to prevent desynchronization.

Under this paradigm:
- A single [[Decoration Layout Engine]] is provided.
- The Engine maps declarative logic to produce absolute integer boundaries (a [[DecorationLayout]]).
- Both visual presentation and logical interaction domains strictly consume the exact same rectangles from the layout struct.
