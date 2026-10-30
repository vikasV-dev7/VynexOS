#pragma once
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

/// Builds the metrics for one window from its frame style and current bounds.
[[nodiscard]] WindowFrameMetrics make_frame_metrics(const FrameStyle& style, const Rect& decorated_bounds, const Rect& client_bounds);

/**
 * Calculates the completely resolved, canonical geometry for all window decoration components. The decorator (drawing)
 * and the window manager (hit-testing) both use this one function, so what is drawn is what is clickable.
 *
 * All rectangles are relative to the decorated bounds.
 *  - visible_frame: the drawn frame, i.e. the bounds inset by metrics.resize_inset (the invisible resize band).
 *  - title_bar:     the part of the top margin inside the visible frame.
 *  - client_area:   the bounds shrunk by the margins.
 *  - Resize zones:  a ring of (resize_inset + border_thickness) around the bounds: four edges plus four corner squares.
 *                   With resize_inset == 0 and no margins this is the original behaviour.
 *  - Buttons:       right-aligned in the title bar, ordered minimize | maximize | close (left to right), vertically
 *                   placed at button_margin_top. They are dropped in the order minimize, maximize, close when the title
 *                   bar is too narrow to keep a usable title area, and never overlap the resize ring.
 */
DecorationLayout calculate_decoration_layout(const WindowFrameMetrics& metrics);

} // namespace vynexos::desktop
