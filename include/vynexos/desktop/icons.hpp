#pragma once
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/painter.hpp"

namespace vynexos::desktop {

/// Glyphs drawn with the anti-aliased painter on a 16 px design grid (scaled to the target box).
enum class IconId : uint8_t { Minimize, Maximize, Restore, Close };

/// Draws `icon` centred in `box` using `color`. Stroke weight follows the box size, so every icon in a given size
/// has the same visual weight.
void draw_icon(Painter& painter, IconId icon, const Rect& box, Color color) noexcept;

} // namespace vynexos::desktop
