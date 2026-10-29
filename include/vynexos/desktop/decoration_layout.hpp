#pragma once
#include "vynexos/desktop/desktop_types.hpp"

namespace vynexos::desktop {

// Calculates the completely resolved, canonical geometry for all window decoration components.
DecorationLayout calculate_decoration_layout(const WindowFrameMetrics& metrics);

} // namespace vynexos::desktop
