#pragma once
// Shared component painting for the VynexOS "Signal" design system.
//
// Three small layers, each usable on its own:
//   1. ControlState      what the user is doing to a control. Owned by whoever owns the control (the shell, a window
//                        decorator, an application); nothing here stores it.
//   2. Style resolvers   pure functions (theme, variant, state) -> BoxStyle. This is the ONE place that decides how a
//                        state changes a colour, so every component hovers, presses, disables and selects the same way.
//   3. Painters          stateless functions that draw a resolved style with the Painter and TextRenderer.
//
// The state model (identical for every component):
//   hovered   the base fill with theme.colors.hover_overlay composited over it
//   pressed   the base fill with theme.colors.pressed_overlay composited over it (takes precedence over hovered)
//   focused   a focus ring outside the control (keyboard focus); the control itself does not change
//   disabled  text_disabled content, no overlays, no focus ring, the quietest border (takes precedence over all)
//   selected  the accent_subtle tint and, for rows, the accent keyline
//
// Geometry invariant: a component's fill and border always come from one rectangle in one Painter::draw_box call, so a
// border can never enclose an area its fill does not cover.
//
// Known limitation (focus rings): the ring is drawn OUTSIDE the control (2 px gap + 2 px ring), so it is clipped
// wherever it would fall outside the Painter's clip or the surface: a focused control flush against the edge of its
// surface shows no ring on that side. Nothing here compensates; the layout that places a focusable control must leave
// focus_ring_bounds() of room around it. Applications do not yet have keyboard-focusable controls, so no current
// screen is affected. Planned: Stage 7.4 (widgets with keyboard focus are laid out with that room) and Stage 7.8
// (keyboard focus rings everywhere, with a test that no ring is clipped).
//
// Nothing here allocates, keeps state or knows about windows, applications or input.
#include <cstdint>
#include <string_view>

#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/icons.hpp"
#include "vynexos/desktop/paint_types.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/text_renderer.hpp"
#include "vynexos/desktop/theme.hpp"

namespace vynexos::desktop {

/// What the user is doing to a control right now.
struct ControlState {
    bool hovered{false};
    bool pressed{false};
    bool focused{false};  ///< has the keyboard focus
    bool disabled{false};
    bool selected{false}; ///< the chosen row, the active item, a toggled button
};

/// The resolved look of a box-shaped component: everything a painter needs, nothing it has to decide.
struct BoxStyle {
    Color fill{0, 0, 0, 0};
    Color border{0, 0, 0, 0};
    float border_width{0.0f};
    CornerRadii radii{};
    Color content{0, 0, 0, 0}; ///< text and icon colour
};

enum class ButtonVariant : uint8_t {
    Default,     ///< raised surface with a border
    Primary,     ///< accent fill: at most one per window
    Destructive, ///< danger fill: an action that cannot be undone
};

enum class PanelVariant : uint8_t {
    Surface, ///< window body and application backgrounds: flat, square, no border
    Sunken,  ///< wells: inputs, lists, terminal bodies
    Raised,  ///< cards
    Overlay, ///< launcher, menus, popovers, notifications
};

enum class Orientation : uint8_t { Horizontal, Vertical };

/// Which edge of an item its selection keyline sits on.
enum class IndicatorEdge : uint8_t {
    Leading, ///< rows: the left edge
    Bottom,  ///< tabs, taskbar items: the bottom edge
};

// --- The shared state model --------------------------------------------------------------------------------------

/// `base` as a control in `state` shows it: the pressed or hover overlay composited over it (pressed wins), or `base`
/// unchanged when idle or disabled.
[[nodiscard]] Color state_fill(const Theme& theme, Color base, const ControlState& state) noexcept;

// --- Style resolvers -----------------------------------------------------------------------------------------------

[[nodiscard]] BoxStyle button_style(const Theme& theme, ButtonVariant variant, const ControlState& state) noexcept;
[[nodiscard]] BoxStyle icon_button_style(const Theme& theme, const ControlState& state) noexcept;
[[nodiscard]] BoxStyle panel_style(const Theme& theme, PanelVariant variant) noexcept;
[[nodiscard]] BoxStyle list_row_style(const Theme& theme, const ControlState& state) noexcept;

// --- Geometry ------------------------------------------------------------------------------------------------------

/// The rectangle a focus ring occupies around `control`: the control grown by the ring's offset and width on every side.
[[nodiscard]] Rect focus_ring_bounds(const Theme& theme, const Rect& control) noexcept;

/// The keyline that marks `item` as selected: a selection_indicator-thick bar on the given edge, inset by the control
/// radius so it stays inside a rounded item. Empty when the item is too small to hold it.
[[nodiscard]] Rect selection_indicator_rect(const Theme& theme, const Rect& item, IndicatorEdge edge) noexcept;

/// The one-pixel line a separator draws inside `bounds`: centred across the bounds, running their full length.
[[nodiscard]] Rect separator_rect(const Theme& theme, const Rect& bounds, Orientation orientation) noexcept;

// --- Painters ------------------------------------------------------------------------------------------------------

/// Fill and border of `style` from the one rectangle `rect`.
void paint_box(Painter& painter, const Rect& rect, const BoxStyle& style) noexcept;

/// The keyboard focus ring around a control with the given corner radii (see focus_ring_bounds). Drawn outside the
/// control, so the caller's clip and the surface must leave room for it: it is clipped, not moved, where they do not
/// (see the known limitation in the file header).
void paint_focus_ring(Painter& painter, const Theme& theme, const Rect& control, const CornerRadii& radii) noexcept;

/// A push button filling `rect`. The label is centred, in the body_strong style, and shortened with an ellipsis when it
/// does not fit; `text` may be null (no label is drawn).
void paint_button(Painter& painter, const Theme& theme, const TextRenderer* text, const Rect& rect, std::string_view label,
                  ButtonVariant variant = ButtonVariant::Default, const ControlState& state = {}) noexcept;

/// A borderless button showing one icon, centred in `rect`. `state.selected` is the toggled-on look.
void paint_icon_button(Painter& painter, const Theme& theme, const Rect& rect, IconId icon, const ControlState& state = {}) noexcept;

void paint_panel(Painter& painter, const Theme& theme, const Rect& rect, PanelVariant variant = PanelVariant::Surface) noexcept;

/// One row of a list filling `rect`: state fill, label (body style, left aligned, ellipsized) and, when selected, the
/// leading keyline. `text` may be null (no label is drawn).
void paint_list_row(Painter& painter, const Theme& theme, const TextRenderer* text, const Rect& rect, std::string_view label,
                    const ControlState& state = {}) noexcept;

/// The selection keyline of `item` (see selection_indicator_rect): accent, or text_disabled for a disabled item.
void paint_selection_indicator(Painter& painter, const Theme& theme, const Rect& item, IndicatorEdge edge,
                               const ControlState& state = {}) noexcept;

/// A separator line inside `bounds` (see separator_rect).
void paint_separator(Painter& painter, const Theme& theme, const Rect& bounds, Orientation orientation) noexcept;

} // namespace vynexos::desktop
