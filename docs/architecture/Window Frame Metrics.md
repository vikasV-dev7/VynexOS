# Window Frame Metrics

`WindowFrameMetrics` encapsulates all the variables needed by the [[Decoration Layout Engine]] to calculate the [[DecorationLayout]].

## Current Attributes
- `decorated_bounds`: The canonical absolute geometry of the window including all borders.
- `client_bounds`: The canonical absolute geometry of the application content.
- `margins`: The space reserved by the window system on each edge of the client area.
- Internal variables like `title_bar_height`, `border_thickness`, `button_size`, `button_spacing`, `button_margin_top`, `button_margin_right`.

## Extensibility
Future additions such as DPI scale factors or dynamic theme metrics will be injected into this structure. The layout generator (`calculate_decoration_layout`) will incorporate them, automatically propagating the adjustments to both the renderer and the hit-testing logic without architectural changes.
