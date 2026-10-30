#include "vynexos/desktop/icons.hpp"

namespace vynexos::desktop {

namespace {

constexpr float kGrid = 16.0f;
constexpr float kStroke = 1.5f;

} // namespace

void draw_icon(Painter& painter, IconId icon, const Rect& box, Color color) noexcept {
    // Map the 16 px design grid onto the centre of the box (no scaling: icons stay crisp at their native size).
    const float ox = static_cast<float>(box.x) + (static_cast<float>(box.width) - kGrid) * 0.5f;
    const float oy = static_cast<float>(box.y) + (static_cast<float>(box.height) - kGrid) * 0.5f;
    auto px = [&](float v) { return ox + v; };
    auto py = [&](float v) { return oy + v; };

    switch (icon) {
    case IconId::Minimize:
        painter.draw_line(px(3.5f), py(8.5f), px(12.5f), py(8.5f), kStroke, color);
        break;
    case IconId::Maximize:
        painter.stroke_rounded_rect({static_cast<int32_t>(ox) + 3, static_cast<int32_t>(oy) + 3, 10, 10}, CornerRadii::uniform(1.5f), kStroke, color);
        break;
    case IconId::Restore: {
        // Front square plus the visible corner of the square behind it.
        painter.stroke_rounded_rect({static_cast<int32_t>(ox) + 2, static_cast<int32_t>(oy) + 5, 9, 9}, CornerRadii::uniform(1.5f), kStroke, color);
        painter.draw_line(px(5.5f), py(5.0f), px(5.5f), py(2.75f), kStroke, color);
        painter.draw_line(px(5.0f), py(2.75f), px(13.0f), py(2.75f), kStroke, color);
        painter.draw_line(px(13.25f), py(2.75f), px(13.25f), py(10.0f), kStroke, color);
        painter.draw_line(px(13.0f), py(10.0f), px(11.0f), py(10.0f), kStroke, color);
        break;
    }
    case IconId::Close:
        painter.draw_line(px(4.0f), py(4.0f), px(12.0f), py(12.0f), kStroke, color);
        painter.draw_line(px(12.0f), py(4.0f), px(4.0f), py(12.0f), kStroke, color);
        break;
    }
}

} // namespace vynexos::desktop
