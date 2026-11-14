// Shared component painting (components.hpp): the state model, the style resolvers and the painters.
//
// Covers, in the dark and the light theme: every component's default, hovered, pressed, focused, disabled and selected
// look; exact fill and border pixels; the invariant that a border never encloses pixels its fill does not cover, at any
// size; labels (centred, inside, ellipsized, never a foreign colour); clipping and degenerate rectangles; bounds safety
// under a randomised fuzz with guard bytes; the legacy toolkit entry points; and that painting allocates nothing.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <vector>

#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/desktop/components.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/text_renderer.hpp"
#include "vynexos/desktop/theme.hpp"

#include "test_check.hpp"

using namespace vynexos::desktop;

// ---------------------------------------------------------------------------------------------------------
// Allocation counter: lets a test prove that painting performs no heap allocation.
// ---------------------------------------------------------------------------------------------------------
namespace {
std::atomic<bool> g_count_allocs{false};
std::atomic<size_t> g_alloc_count{0};
} // namespace

void* operator new(size_t size) {
    if (g_count_allocs.load(std::memory_order_relaxed)) g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t size) {
    if (g_count_allocs.load(std::memory_order_relaxed)) g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

namespace {

// ---------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------
bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool same_radii(const CornerRadii& r, float v) { return r.top_left == v && r.top_right == v && r.bottom_right == v && r.bottom_left == v; }

bool same_style(const BoxStyle& a, const BoxStyle& b) {
    return same(a.fill, b.fill) && same(a.border, b.border) && a.border_width == b.border_width && same(a.content, b.content) &&
           a.radii.top_left == b.radii.top_left && a.radii.top_right == b.radii.top_right && a.radii.bottom_right == b.radii.bottom_right &&
           a.radii.bottom_left == b.radii.bottom_left;
}

Color px(SoftwareSurface& s, int x, int y) {
    auto m = s.map_pixels();
    const size_t i = (static_cast<size_t>(y) * s.width() + static_cast<size_t>(x)) * 4;
    const Color c{m[i], m[i + 1], m[i + 2], m[i + 3]};
    s.unmap_pixels();
    return c;
}

std::vector<uint8_t> snapshot(SoftwareSurface& s) {
    auto m = s.map_pixels();
    std::vector<uint8_t> copy(m.begin(), m.end());
    s.unmap_pixels();
    return copy;
}

void fill_all(SoftwareSurface& s, Color c) {
    Painter p(s);
    p.fill_rect({0, 0, s.width(), s.height()}, c);
}

const TextRenderer* text() {
    const TextRenderer* t = TextRenderer::shared_default();
    CHECK(t != nullptr);
    return t;
}

struct Ink {
    int x0{1 << 20}, y0{1 << 20}, x1{-1}, y1{-1};
    size_t pixels{0};
    [[nodiscard]] bool any() const { return pixels > 0; }
};

/// Pixels inside `area` that are not exactly `background`: what was drawn on top of a flat fill.
Ink ink_over(SoftwareSurface& s, const Rect& area, Color background) {
    Ink ink;
    auto m = s.map_pixels();
    for (int y = area.y; y < area.y + static_cast<int>(area.height); ++y) {
        for (int x = area.x; x < area.x + static_cast<int>(area.width); ++x) {
            if (x < 0 || y < 0 || x >= static_cast<int>(s.width()) || y >= static_cast<int>(s.height())) continue;
            const size_t i = (static_cast<size_t>(y) * s.width() + static_cast<size_t>(x)) * 4;
            if (m[i] == background.r && m[i + 1] == background.g && m[i + 2] == background.b && m[i + 3] == background.a) continue;
            ink.x0 = std::min(ink.x0, x);
            ink.y0 = std::min(ink.y0, y);
            ink.x1 = std::max(ink.x1, x);
            ink.y1 = std::max(ink.y1, y);
            ++ink.pixels;
        }
    }
    s.unmap_pixels();
    return ink;
}

/// True when every channel of every pixel in `area` lies between the two colours (inclusive, +-1 for rounding): the
/// label is the content colour blended onto the fill and introduces no other colour.
bool only_between(SoftwareSurface& s, const Rect& area, Color a, Color b) {
    auto m = s.map_pixels();
    bool ok = true;
    for (int y = area.y; y < area.y + static_cast<int>(area.height) && ok; ++y) {
        for (int x = area.x; x < area.x + static_cast<int>(area.width) && ok; ++x) {
            const size_t i = (static_cast<size_t>(y) * s.width() + static_cast<size_t>(x)) * 4;
            const uint8_t lo[3] = {std::min(a.r, b.r), std::min(a.g, b.g), std::min(a.b, b.b)};
            const uint8_t hi[3] = {std::max(a.r, b.r), std::max(a.g, b.g), std::max(a.b, b.b)};
            for (size_t ch = 0; ch < 3; ++ch) {
                if (m[i + ch] + 1 < lo[ch] || m[i + ch] > hi[ch] + 1) ok = false;
            }
        }
    }
    s.unmap_pixels();
    return ok;
}

/// Where a control's label can be, clear of its border and of the anti-aliased pixels of its rounded corners: inside
/// the border horizontally, and between the corner arcs vertically.
Rect label_area(const Rect& r) {
    return {r.x + 2, r.y + 6, r.width - 4, r.height - 12};
}

const ThemeId kThemes[] = {ThemeId::Dark, ThemeId::Light};
const char* name_of(ThemeId id) { return id == ThemeId::Dark ? "dark" : "light"; }

ControlState with(bool hovered = false, bool pressed = false, bool focused = false, bool disabled = false, bool selected = false) {
    ControlState s;
    s.hovered = hovered;
    s.pressed = pressed;
    s.focused = focused;
    s.disabled = disabled;
    s.selected = selected;
    return s;
}
const ControlState kIdle{};
const ControlState kHovered = with(true);
const ControlState kPressed = with(false, true);
const ControlState kHoveredPressed = with(true, true);
const ControlState kFocused = with(false, false, true);
const ControlState kDisabled = with(false, false, false, true);
const ControlState kSelected = with(false, false, false, false, true);

// ---------------------------------------------------------------------------------------------------------
// The shared state model and the style resolvers
// ---------------------------------------------------------------------------------------------------------
void test_state_model() {
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;
        for (const Color base : {c.surface, c.surface_raised, c.danger_solid, Color{0, 0, 0, 0}, c.accent_subtle}) {
            CHECK(same(state_fill(t, base, kIdle), base));
            CHECK(same(state_fill(t, base, kFocused), base));                       // focus is a ring, not a fill
            CHECK(same(state_fill(t, base, kSelected), base));                      // selection is the caller's base
            CHECK(same(state_fill(t, base, kHovered), blend_over(c.hover_overlay, base)));
            CHECK(same(state_fill(t, base, kPressed), blend_over(c.pressed_overlay, base)));
            CHECK(same(state_fill(t, base, kHoveredPressed), blend_over(c.pressed_overlay, base))); // pressed wins
            CHECK(same(state_fill(t, base, with(true, true, true, true, true)), base));            // disabled wins over all
        }
        // The overlays move a surface in the theme's direction (lighter on dark, darker on light), pressed further.
        const double idle = relative_luminance(c.surface), hov = relative_luminance(state_fill(t, c.surface, kHovered)),
                     pre = relative_luminance(state_fill(t, c.surface, kPressed));
        CHECK(id == ThemeId::Dark ? (idle < hov && hov < pre) : (idle > hov && hov > pre));
    }
    vynex_test::context().clear();
    std::printf("test_state_model passed\n");
}

void test_button_styles() {
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;

        const BoxStyle idle = button_style(t, ButtonVariant::Default, kIdle);
        CHECK(same(idle.fill, c.surface_raised) && same(idle.border, c.border) && idle.border_width == 1.0f);
        CHECK(same(idle.content, c.text_primary) && same_radii(idle.radii, 6.0f));
        CHECK(same(button_style(t, ButtonVariant::Default, kHovered).fill, blend_over(c.hover_overlay, c.surface_raised)));
        CHECK(same(button_style(t, ButtonVariant::Default, kPressed).fill, blend_over(c.pressed_overlay, c.surface_raised)));
        CHECK(same(button_style(t, ButtonVariant::Default, kHoveredPressed).fill, blend_over(c.pressed_overlay, c.surface_raised)));
        CHECK(same_style(button_style(t, ButtonVariant::Default, kFocused), idle)); // the ring is the only focus cue
        const BoxStyle toggled = button_style(t, ButtonVariant::Default, kSelected);
        CHECK(same(toggled.fill, blend_over(c.accent_subtle, c.surface_raised)) && same(toggled.border, c.border));
        CHECK(same(button_style(t, ButtonVariant::Default, with(true, false, false, false, true)).fill,
                   blend_over(c.hover_overlay, blend_over(c.accent_subtle, c.surface_raised))));

        const BoxStyle primary = button_style(t, ButtonVariant::Primary, kIdle);
        CHECK(same(primary.fill, c.accent) && same(primary.content, c.text_on_accent) && primary.border_width == 0.0f && primary.border.a == 0);
        CHECK(same(button_style(t, ButtonVariant::Primary, kHovered).fill, c.accent_hover));
        CHECK(same(button_style(t, ButtonVariant::Primary, kPressed).fill, c.accent_pressed));
        CHECK(same(button_style(t, ButtonVariant::Primary, kHoveredPressed).fill, c.accent_pressed));
        CHECK(same_radii(primary.radii, 6.0f));

        const BoxStyle destructive = button_style(t, ButtonVariant::Destructive, kIdle);
        CHECK(same(destructive.fill, c.danger_solid) && same(destructive.content, c.text_on_danger) && destructive.border_width == 0.0f);
        CHECK(same(button_style(t, ButtonVariant::Destructive, kHovered).fill, blend_over(c.hover_overlay, c.danger_solid)));
        CHECK(same(button_style(t, ButtonVariant::Destructive, kPressed).fill, c.danger_pressed));

        // Disabled is one look for every variant, whatever else is going on.
        const BoxStyle disabled = button_style(t, ButtonVariant::Default, kDisabled);
        CHECK(same(disabled.fill, c.surface_raised) && same(disabled.border, c.border_subtle) && disabled.border_width == 1.0f);
        CHECK(same(disabled.content, c.text_disabled));
        for (const ButtonVariant v : {ButtonVariant::Default, ButtonVariant::Primary, ButtonVariant::Destructive}) {
            CHECK(same_style(button_style(t, v, kDisabled), disabled));
            CHECK(same_style(button_style(t, v, with(true, true, true, true, true)), disabled));
        }

        // A label is readable (AA, 4.5:1) on every enabled fill of every variant, in every state.
        for (const ButtonVariant v : {ButtonVariant::Default, ButtonVariant::Primary, ButtonVariant::Destructive}) {
            for (const ControlState& s : {kIdle, kHovered, kPressed, kHoveredPressed, kFocused, kSelected, with(true, false, true)}) {
                const BoxStyle style = button_style(t, v, s);
                CHECK(contrast_ratio(style.content, with_alpha(style.fill, 255)) >= 4.5);
            }
        }
    }
    vynex_test::context().clear();
    std::printf("test_button_styles passed\n");
}

// The destructive button in each of its five states, in both themes: resolved style, painted pixels and label contrast.
void test_destructive_button_in_every_state() {
    struct Expected {
        ThemeId id;
        Color rest, hover, pressed;
        double rest_min, hover_min, pressed_min;
    };
    const Expected cases[] = {
        {ThemeId::Dark, rgb(0xCA383E), rgb(0xCE464C), rgb(0xC13338), 5.0, 4.5, 5.5}, // hover: 4.56:1 (was 4.37 with #D03A40)
        {ThemeId::Light, rgb(0xC8323A), rgb(0xBE2F37), rgb(0xA0282E), 5.2, 5.7, 7.3}, // the light overlay darkens the fill
    };
    const Rect rect{30, 24, 100, 32};
    for (const Expected& e : cases) {
        vynex_test::context() = std::string(name_of(e.id)) + " destructive button";
        const Theme& t = theme_for(e.id);
        const ColorTokens& c = t.colors;

        // The resolved fills and label colour.
        const BoxStyle rest = button_style(t, ButtonVariant::Destructive, kIdle);
        const BoxStyle hover = button_style(t, ButtonVariant::Destructive, kHovered);
        const BoxStyle pressed = button_style(t, ButtonVariant::Destructive, kPressed);
        const BoxStyle focused = button_style(t, ButtonVariant::Destructive, kFocused);
        const BoxStyle disabled = button_style(t, ButtonVariant::Destructive, kDisabled);
        CHECK(same(rest.fill, e.rest) && same(hover.fill, e.hover) && same(pressed.fill, e.pressed));
        CHECK(same(rest.fill, c.danger_solid) && same(pressed.fill, c.danger_pressed));
        CHECK(same(rest.content, c.text_on_danger) && same(hover.content, c.text_on_danger) && same(pressed.content, c.text_on_danger));
        CHECK(same_style(focused, rest));        // focus adds a ring, nothing else
        CHECK(same(disabled.fill, c.surface_raised) && same(disabled.border, c.border_subtle) && same(disabled.content, c.text_disabled));
        CHECK(same_style(button_style(t, ButtonVariant::Destructive, with(true, true, true, true, true)), disabled)); // disabled wins

        // Label contrast in every enabled state (AA), with the expected margins; disabled text is exempt but legible.
        CHECK(contrast_ratio(rest.content, rest.fill) >= e.rest_min);
        CHECK(contrast_ratio(hover.content, hover.fill) >= e.hover_min && contrast_ratio(hover.content, hover.fill) >= 4.5);
        CHECK(contrast_ratio(pressed.content, pressed.fill) >= e.pressed_min);
        CHECK(contrast_ratio(button_style(t, ButtonVariant::Destructive, kHoveredPressed).content, button_style(t, ButtonVariant::Destructive, kHoveredPressed).fill) >= 4.5);
        CHECK(contrast_ratio(disabled.content, disabled.fill) >= 2.0);
        // Hover is not weakened: it is visibly different from rest, and in the direction of the theme.
        CHECK(!same(hover.fill, rest.fill));
        CHECK(e.id == ThemeId::Dark ? relative_luminance(hover.fill) > relative_luminance(rest.fill)
                                    : relative_luminance(hover.fill) < relative_luminance(rest.fill));

        // The painted button shows exactly those fills (away from the label), and the focused one a ring.
        struct State {
            ControlState state;
            const BoxStyle* style;
            bool ring;
        };
        for (const State& s : {State{kIdle, &rest, false}, State{kHovered, &hover, false}, State{kPressed, &pressed, false}, State{kFocused, &focused, true},
                               State{kDisabled, &disabled, false}, State{with(false, false, true, true), &disabled, false}}) {
            SoftwareSurface surface(160, 80);
            { Painter p(surface); paint_button(p, t, text(), rect, "Delete", ButtonVariant::Destructive, s.state); }
            CHECK(same(px(surface, rect.x + 4, rect.y + 16), s.style->fill));
            CHECK(same(px(surface, rect.x + 95, rect.y + 16), s.style->fill));
            CHECK(same(px(surface, rect.x - 4, rect.y + 16), c.focus_ring) == s.ring);  // the ring only when focused and enabled
            const Ink label = ink_over(surface, label_area(rect), s.style->fill);
            CHECK(label.any() && only_between(surface, label_area(rect), s.style->fill, s.style->content));
        }
    }
    vynex_test::context().clear();
    std::printf("test_destructive_button_in_every_state passed\n");
}

void test_icon_button_panel_and_row_styles() {
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;

        // Icon button: no fill and no border at rest; the overlays appear on their own; toggled is the accent tint.
        const BoxStyle icon = icon_button_style(t, kIdle);
        CHECK(icon.fill.a == 0 && icon.border.a == 0 && icon.border_width == 0.0f && same(icon.content, c.text_primary) && same_radii(icon.radii, 6.0f));
        CHECK(same(icon_button_style(t, kHovered).fill, c.hover_overlay));
        CHECK(same(icon_button_style(t, kPressed).fill, c.pressed_overlay));
        const BoxStyle toggled = icon_button_style(t, kSelected);
        CHECK(same(toggled.fill, c.accent_subtle) && same(toggled.content, c.accent));
        CHECK(same(icon_button_style(t, with(true, false, false, false, true)).fill, blend_over(c.hover_overlay, c.accent_subtle)));
        const BoxStyle icon_disabled = icon_button_style(t, with(true, true, true, true, true));
        CHECK(icon_disabled.fill.a == 0 && same(icon_disabled.content, c.text_disabled));
        CHECK(same_style(icon_button_style(t, kFocused), icon));

        // Panels: one per elevation.
        const BoxStyle surface = panel_style(t, PanelVariant::Surface);
        CHECK(same(surface.fill, c.surface) && surface.border_width == 0.0f && same_radii(surface.radii, 0.0f) && surface.fill.a == 255);
        const BoxStyle sunken = panel_style(t, PanelVariant::Sunken);
        CHECK(same(sunken.fill, c.surface_sunken) && same(sunken.border, c.border_subtle) && sunken.border_width == 1.0f && same_radii(sunken.radii, 6.0f));
        const BoxStyle raised = panel_style(t, PanelVariant::Raised);
        CHECK(same(raised.fill, c.surface_raised) && same(raised.border, c.border) && raised.border_width == 1.0f && same_radii(raised.radii, 8.0f));
        const BoxStyle overlay = panel_style(t, PanelVariant::Overlay);
        CHECK(same(overlay.fill, c.surface_overlay) && same(overlay.border, c.border) && overlay.border_width == 1.0f && same_radii(overlay.radii, 8.0f));
        for (const PanelVariant v : {PanelVariant::Surface, PanelVariant::Sunken, PanelVariant::Raised, PanelVariant::Overlay}) {
            CHECK(same(panel_style(t, v).content, c.text_primary));
        }

        // List rows: transparent at rest (the list's own background shows), tinted when selected.
        const BoxStyle row = list_row_style(t, kIdle);
        CHECK(row.fill.a == 0 && row.border_width == 0.0f && same(row.content, c.text_primary) && same_radii(row.radii, 6.0f));
        CHECK(same(list_row_style(t, kHovered).fill, c.hover_overlay));
        CHECK(same(list_row_style(t, kPressed).fill, c.pressed_overlay));
        CHECK(same(list_row_style(t, kSelected).fill, c.accent_subtle));
        CHECK(same(list_row_style(t, with(true, false, false, false, true)).fill, blend_over(c.hover_overlay, c.accent_subtle)));
        CHECK(same(list_row_style(t, with(false, true, false, false, true)).fill, blend_over(c.pressed_overlay, c.accent_subtle)));
        const BoxStyle row_disabled = list_row_style(t, with(true, true, true, true, true));
        CHECK(row_disabled.fill.a == 0 && same(row_disabled.content, c.text_disabled));
        CHECK(same_style(list_row_style(t, kFocused), row));
    }
    vynex_test::context().clear();
    std::printf("test_icon_button_panel_and_row_styles passed\n");
}

// ---------------------------------------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------------------------------------
bool is(const Rect& r, int32_t x, int32_t y, uint32_t w, uint32_t h) { return r.x == x && r.y == y && r.width == w && r.height == h; }

void test_geometry_helpers() {
    const Theme& t = default_theme();
    constexpr int32_t kMinI = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMaxI = std::numeric_limits<int32_t>::max();
    constexpr uint32_t kMaxU = std::numeric_limits<uint32_t>::max();

    // Focus ring: 2 px gap + 2 px ring on every side.
    CHECK(is(focus_ring_bounds(t, {30, 24, 100, 32}), 26, 20, 108, 40));
    CHECK(focus_ring_bounds(t, {30, 24, 0, 32}).width == 0 && focus_ring_bounds(t, {30, 24, 100, 0}).height == 0);
    const Rect far = focus_ring_bounds(t, {kMinI, kMaxI, kMaxU, kMaxU}); // saturates instead of wrapping
    CHECK(far.x == kMinI && far.y == kMaxI - 4 && far.width == kMaxU && far.height == kMaxU);

    // Keyline: selection_indicator (2 px) thick, inset by the control radius (6 px) so it stays inside a rounded item.
    CHECK(is(selection_indicator_rect(t, {10, 20, 200, 32}, IndicatorEdge::Leading), 10, 26, 2, 20));
    CHECK(is(selection_indicator_rect(t, {10, 20, 200, 32}, IndicatorEdge::Bottom), 16, 50, 188, 2));
    CHECK(is(selection_indicator_rect(t, {-5, -9, 40, 28}, IndicatorEdge::Leading), -5, -3, 2, 16));
    // Too small to hold it: nothing, rather than a bar poking out of the item.
    CHECK(selection_indicator_rect(t, {0, 0, 200, 12}, IndicatorEdge::Leading).height == 0);
    CHECK(selection_indicator_rect(t, {0, 0, 1, 32}, IndicatorEdge::Leading).width == 0);
    CHECK(selection_indicator_rect(t, {0, 0, 12, 32}, IndicatorEdge::Bottom).width == 0);
    CHECK(selection_indicator_rect(t, {0, 0, 200, 1}, IndicatorEdge::Bottom).height == 0);
    CHECK(selection_indicator_rect(t, {0, 0, 0, 0}, IndicatorEdge::Leading).width == 0);
    const Rect huge = selection_indicator_rect(t, {kMaxI, kMaxI, kMaxU, kMaxU}, IndicatorEdge::Bottom);
    CHECK(huge.height == 2 && huge.x == kMaxI && huge.y == kMaxI); // clamped, not wrapped

    // Separator: one pixel, centred across its bounds, running their full length.
    CHECK(is(separator_rect(t, {0, 0, 100, 9}, Orientation::Horizontal), 0, 4, 100, 1));
    CHECK(is(separator_rect(t, {5, 7, 100, 1}, Orientation::Horizontal), 5, 7, 100, 1));
    CHECK(is(separator_rect(t, {5, 7, 9, 60}, Orientation::Vertical), 9, 7, 1, 60));
    CHECK(is(separator_rect(t, {5, 7, 2, 60}, Orientation::Vertical), 5, 7, 1, 60));
    CHECK(separator_rect(t, {5, 7, 0, 60}, Orientation::Vertical).width == 0);
    CHECK(separator_rect(t, {5, 7, 100, 0}, Orientation::Horizontal).height == 0);
    std::printf("test_geometry_helpers passed\n");
}

// ---------------------------------------------------------------------------------------------------------
// Painted pixels
// ---------------------------------------------------------------------------------------------------------
void test_button_pixels_in_every_state() {
    const Rect rect{30, 24, 100, 32};
    for (const ThemeId id : kThemes) {
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;
        for (const ButtonVariant variant : {ButtonVariant::Default, ButtonVariant::Primary, ButtonVariant::Destructive}) {
            for (const ControlState& state : {kIdle, kHovered, kPressed, kHoveredPressed, kDisabled, kSelected, kFocused,
                                              with(false, false, true, true, false)}) {
                vynex_test::context() = std::string(name_of(id)) + " button variant " + std::to_string(static_cast<int>(variant)) +
                                        (state.hovered ? " hovered" : "") + (state.pressed ? " pressed" : "") + (state.disabled ? " disabled" : "") +
                                        (state.selected ? " selected" : "") + (state.focused ? " focused" : "");
                SoftwareSurface s(160, 80);
                {
                    Painter p(s);
                    paint_button(p, t, text(), rect, "Save", variant, state);
                }
                const BoxStyle style = button_style(t, variant, state);

                // The fill, exactly, where there is no label (inside the padding, at the left and right ends).
                CHECK(same(px(s, rect.x + 4, rect.y + 16), style.fill));
                CHECK(same(px(s, rect.x + 95, rect.y + 16), style.fill));
                CHECK(same(px(s, rect.x + 50, rect.y + 3), style.fill));
                // The border, exactly, along all four straight edges; or the fill there when the variant has none.
                const Color edge = style.border_width > 0.0f ? style.border : style.fill;
                CHECK(same(px(s, rect.x + 50, rect.y), edge) && same(px(s, rect.x + 50, rect.y + 31), edge));
                CHECK(same(px(s, rect.x, rect.y + 16), edge) && same(px(s, rect.x + 99, rect.y + 16), edge));
                if (style.border_width > 0.0f) CHECK(same(px(s, rect.x + 50, rect.y + 1), style.fill)); // one pixel thick
                // Rounded: the corner tips are not painted by the control. (With a focus ring, the ring's inner edge is
                // concentric with the corner, 2 px out, and its anti-aliasing reaches the tip faintly: about a quarter
                // of the ring's own alpha, never the control's fill or border.)
                const bool has_ring = state.focused && !state.disabled;
                for (const Color tip : {px(s, rect.x, rect.y), px(s, rect.x + 99, rect.y + 31)}) {
                    CHECK(has_ring ? (tip.a < 0x50 && (tip.a == 0 || (tip.r == c.focus_ring.r && tip.g == c.focus_ring.g))) : tip.a == 0);
                }

                // The label: present, inside the padding, centred, and made of the content colour on the fill only.
                const Rect interior = label_area(rect);
                const Ink label = ink_over(s, interior, style.fill);
                CHECK(label.any() && label.pixels > 40);
                CHECK(label.x0 >= rect.x + t.spacing.md && label.x1 < rect.x + static_cast<int>(rect.width) - t.spacing.md);
                CHECK(std::fabs((label.x0 + label.x1 + 1) / 2.0 - (rect.x + rect.width / 2.0)) <= 2.0);
                CHECK(std::fabs((label.y0 + label.y1 + 1) / 2.0 - (rect.y + rect.height / 2.0)) <= 2.0);
                CHECK(only_between(s, interior, style.fill, style.content));
                if (state.disabled) CHECK(same(style.content, c.text_disabled));

                // Focus: a ring of focus_ring, 2 px wide, 2 px away; nothing outside the control otherwise.
                const bool ring = state.focused && !state.disabled;
                const Color ring_px = px(s, rect.x - 4, rect.y + 16);
                CHECK(ring ? same(ring_px, c.focus_ring) : ring_px.a == 0);
                CHECK(ring ? same(px(s, rect.x - 3, rect.y + 16), c.focus_ring) : px(s, rect.x - 3, rect.y + 16).a == 0);
                CHECK(px(s, rect.x - 2, rect.y + 16).a == 0 && px(s, rect.x - 1, rect.y + 16).a == 0);           // the gap
                CHECK(ring == same(px(s, rect.x + 50, rect.y - 4), c.focus_ring) && px(s, rect.x + 50, rect.y - 1).a == 0);
                CHECK(ring == same(px(s, rect.x + 103, rect.y + 16), c.focus_ring));
                CHECK(ring == same(px(s, rect.x + 50, rect.y + 35), c.focus_ring));
                CHECK(px(s, rect.x - 5, rect.y + 16).a == 0 && px(s, rect.x + 50, rect.y - 5).a == 0);            // nothing beyond it
                const Rect bounds = ring ? focus_ring_bounds(t, rect) : rect;
                size_t stray = 0;
                auto m = s.map_pixels();
                for (int y = 0; y < 80; ++y) {
                    for (int x = 0; x < 160; ++x) {
                        const bool inside = x >= bounds.x && x < bounds.x + static_cast<int>(bounds.width) && y >= bounds.y && y < bounds.y + static_cast<int>(bounds.height);
                        if (!inside && m[(static_cast<size_t>(y) * 160 + static_cast<size_t>(x)) * 4 + 3] != 0) ++stray;
                    }
                }
                s.unmap_pixels();
                CHECK(stray == 0);
            }
        }
    }
    vynex_test::context().clear();
    std::printf("test_button_pixels_in_every_state passed\n");
}

void test_icon_button_pixels() {
    const Rect rect{20, 16, 28, 28};
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;
        const Color backdrop = c.surface;

        auto draw = [&](const ControlState& state) {
            auto s = std::make_unique<SoftwareSurface>(70, 60);
            fill_all(*s, backdrop);
            Painter p(*s);
            paint_icon_button(p, t, rect, IconId::Close, state);
            return s;
        };
        // At rest only the icon is drawn, in text_primary.
        auto idle = draw(kIdle);
        CHECK(same(px(*idle, rect.x + 2, rect.y + 14), backdrop) && same(px(*idle, rect.x + 14, rect.y + 2), backdrop));
        const Ink icon = ink_over(*idle, rect, backdrop);
        CHECK(icon.any() && icon.x0 >= rect.x + 6 && icon.x1 <= rect.x + 21 && icon.y0 >= rect.y + 6 && icon.y1 <= rect.y + 21); // the 16 px glyph, centred
        CHECK(same(px(*idle, rect.x + 14, rect.y + 14), c.text_primary)); // the centre of the X
        CHECK(only_between(*idle, rect, backdrop, c.text_primary));
        CHECK(ink_over(*idle, {0, 0, 70, 60}, backdrop).pixels == icon.pixels); // nothing outside the button

        // Hovered and pressed: the overlay over whatever is behind, inside the rounded box.
        auto hovered = draw(kHovered);
        CHECK(same(px(*hovered, rect.x + 2, rect.y + 14), blend_over(c.hover_overlay, backdrop)));
        CHECK(same(px(*hovered, rect.x, rect.y), backdrop)); // the corner tip of the rounded box stays the backdrop
        auto pressed = draw(kPressed);
        CHECK(same(px(*pressed, rect.x + 2, rect.y + 14), blend_over(c.pressed_overlay, backdrop)));

        // Toggled on: the accent tint and an accent icon.
        auto toggled = draw(kSelected);
        CHECK(same(px(*toggled, rect.x + 2, rect.y + 14), blend_over(c.accent_subtle, backdrop)));
        CHECK(same(px(*toggled, rect.x + 14, rect.y + 14), c.accent));

        // Disabled: no fill, a text_disabled icon, and no ring even when focused.
        auto disabled = draw(with(true, true, true, true, true));
        CHECK(same(px(*disabled, rect.x + 2, rect.y + 14), backdrop));
        CHECK(same(px(*disabled, rect.x + 14, rect.y + 14), c.text_disabled));
        CHECK(same(px(*disabled, rect.x - 4, rect.y + 14), backdrop));

        // Focused: the ring around it.
        auto focused = draw(kFocused);
        CHECK(same(px(*focused, rect.x - 4, rect.y + 14), blend_over(c.focus_ring, backdrop)));
        CHECK(same(px(*focused, rect.x - 1, rect.y + 14), backdrop));
    }
    vynex_test::context().clear();
    std::printf("test_icon_button_pixels passed\n");
}

void test_panel_pixels() {
    const Rect rect{10, 8, 80, 50};
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;
        struct Case {
            PanelVariant variant;
            Color fill;
            Color edge;
            bool rounded;
        };
        const Case cases[] = {{PanelVariant::Surface, c.surface, c.surface, false},
                              {PanelVariant::Sunken, c.surface_sunken, c.border_subtle, true},
                              {PanelVariant::Raised, c.surface_raised, c.border, true},
                              {PanelVariant::Overlay, c.surface_overlay, c.border, true}};
        for (const Case& k : cases) {
            SoftwareSurface s(100, 70);
            { Painter p(s); paint_panel(p, t, rect, k.variant); }
            CHECK(same(px(s, 50, 30), k.fill) && same(px(s, 12, 30), k.fill) && same(px(s, 50, 10), k.fill));
            CHECK(same(px(s, 50, 8), k.edge) && same(px(s, 50, 57), k.edge) && same(px(s, 10, 30), k.edge) && same(px(s, 89, 30), k.edge));
            CHECK(k.rounded ? px(s, 10, 8).a == 0 : same(px(s, 10, 8), k.fill)); // the Surface panel is a plain rectangle
            CHECK(px(s, 9, 30).a == 0 && px(s, 90, 30).a == 0 && px(s, 50, 7).a == 0 && px(s, 50, 58).a == 0);
            CHECK(px(s, 50, 30).a == 255); // every panel is opaque: it covers what is under it
        }
    }
    vynex_test::context().clear();
    std::printf("test_panel_pixels passed\n");
}

void test_list_row_pixels() {
    const Rect rect{12, 10, 200, 32};
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;
        const Color backdrop = c.surface;
        auto draw = [&](const ControlState& state, std::string_view label = "Documents") {
            auto s = std::make_unique<SoftwareSurface>(230, 52);
            fill_all(*s, backdrop);
            Painter p(*s);
            paint_list_row(p, t, text(), rect, label, state);
            return s;
        };
        const Rect label_area{rect.x + 4, rect.y + 2, 180, 28};

        // At rest: just the label, left aligned at the row padding, in text_primary on the list's own background.
        auto idle = draw(kIdle);
        const Ink label = ink_over(*idle, {0, 0, 230, 52}, backdrop);
        CHECK(label.any() && label.x0 >= rect.x + t.spacing.sm && label.x0 <= rect.x + t.spacing.sm + 2);
        CHECK(label.x1 < rect.x + 120 && label.y0 >= rect.y + 6 && label.y1 < rect.y + 28);
        CHECK(only_between(*idle, {0, 0, 230, 52}, backdrop, c.text_primary));

        // Hovered / pressed: the overlay across the whole row.
        auto hovered = draw(kHovered);
        CHECK(same(px(*hovered, rect.x + 190, rect.y + 16), blend_over(c.hover_overlay, backdrop)));
        CHECK(same(px(*hovered, rect.x + 100, rect.y + 2), blend_over(c.hover_overlay, backdrop)));
        auto pressed = draw(kPressed);
        CHECK(same(px(*pressed, rect.x + 190, rect.y + 16), blend_over(c.pressed_overlay, backdrop)));

        // Selected: the accent tint and the keyline on the leading edge, exactly where selection_indicator_rect says.
        auto selected = draw(kSelected);
        const Color tint = blend_over(c.accent_subtle, backdrop);
        CHECK(same(px(*selected, rect.x + 190, rect.y + 16), tint));
        const Rect bar = selection_indicator_rect(t, rect, IndicatorEdge::Leading);
        CHECK(bar.x == rect.x && bar.width == 2 && bar.y == rect.y + 6 && bar.height == 20);
        for (int y = bar.y + 1; y < bar.y + static_cast<int>(bar.height) - 1; ++y) {
            CHECK(same(px(*selected, bar.x, y), c.accent) && same(px(*selected, bar.x + 1, y), c.accent));
            CHECK(same(px(*selected, bar.x + 2, y), tint)); // exactly two pixels wide
        }
        CHECK(same(px(*selected, bar.x, bar.y - 2), tint) || px(*selected, bar.x, bar.y - 2).a == 255); // above the keyline: row, not bar
        CHECK(!same(px(*selected, bar.x, bar.y - 2), c.accent) && !same(px(*selected, bar.x, bar.y + static_cast<int>(bar.height) + 1), c.accent));
        CHECK(only_between(*selected, label_area, tint, c.text_primary));
        CHECK(contrast_ratio(c.text_primary, tint) >= 7.0);
        // Selected and hovered: both, composited in that order.
        auto selected_hovered = draw(with(true, false, false, false, true));
        CHECK(same(px(*selected_hovered, rect.x + 190, rect.y + 16), blend_over(blend_over(c.hover_overlay, c.accent_subtle), backdrop)));

        // Disabled: no fill; a muted label; a selected-but-disabled row keeps a muted keyline so the choice stays visible.
        auto disabled = draw(with(true, true, true, true, false));
        CHECK(same(px(*disabled, rect.x + 190, rect.y + 16), backdrop));
        CHECK(only_between(*disabled, {0, 0, 230, 52}, backdrop, c.text_disabled));
        CHECK(ink_over(*disabled, {0, 0, 230, 52}, backdrop).x0 >= rect.x + t.spacing.sm);
        auto disabled_selected = draw(with(false, false, false, true, true));
        CHECK(same(px(*disabled_selected, bar.x, bar.y + 10), c.text_disabled));
        CHECK(same(px(*disabled_selected, rect.x + 190, rect.y + 16), backdrop));

        // Focused: the ring around the row.
        auto focused = draw(kFocused);
        CHECK(same(px(*focused, rect.x - 4, rect.y + 16), blend_over(c.focus_ring, backdrop)));
        CHECK(same(px(*focused, rect.x - 1, rect.y + 16), backdrop));

        // A label too long for the row is shortened, never drawn past the row's padding.
        auto longer = draw(kIdle, "A very long file name that cannot possibly fit in a row of this width.txt");
        const Ink shortened = ink_over(*longer, {0, 0, 230, 52}, backdrop);
        CHECK(shortened.any() && shortened.x1 < rect.x + static_cast<int>(rect.width) - t.spacing.sm + 1 && shortened.x1 > rect.x + 150);
    }
    vynex_test::context().clear();
    std::printf("test_list_row_pixels passed\n");
}

void test_selection_indicator_and_separator_pixels() {
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        const ColorTokens& c = t.colors;

        // Bottom keyline (tabs, taskbar items).
        const Rect item{10, 8, 80, 32};
        SoftwareSurface s(100, 50);
        { Painter p(s); paint_selection_indicator(p, t, item, IndicatorEdge::Bottom); }
        const Rect bar = selection_indicator_rect(t, item, IndicatorEdge::Bottom);
        CHECK(is(bar, 16, 38, 68, 2));
        for (int x = bar.x + 1; x < bar.x + static_cast<int>(bar.width) - 1; ++x) CHECK(same(px(s, x, 38), c.accent) && same(px(s, x, 39), c.accent));
        size_t outside = 0;
        for (int y = 0; y < 50; ++y) {
            for (int x = 0; x < 100; ++x) {
                const bool in_bar = x >= bar.x && x < bar.x + static_cast<int>(bar.width) && y >= bar.y && y < bar.y + static_cast<int>(bar.height);
                if (!in_bar && px(s, x, y).a != 0) ++outside;
            }
        }
        CHECK(outside == 0);
        // Disabled: muted. Too small an item: nothing.
        SoftwareSurface d(100, 50);
        { Painter p(d); paint_selection_indicator(p, t, item, IndicatorEdge::Bottom, kDisabled); }
        CHECK(same(px(d, 50, 38), c.text_disabled));
        SoftwareSurface none(100, 50);
        { Painter p(none); paint_selection_indicator(p, t, {10, 8, 10, 32}, IndicatorEdge::Bottom); }
        CHECK(ink_over(none, {0, 0, 100, 50}, Color{0, 0, 0, 0}).pixels == 0);

        // Separators: exactly one pixel of border_subtle, the full length of the bounds.
        SoftwareSurface h(100, 50);
        { Painter p(h); paint_separator(p, t, {5, 10, 90, 9}, Orientation::Horizontal); }
        const Ink line = ink_over(h, {0, 0, 100, 50}, Color{0, 0, 0, 0});
        CHECK(line.pixels == 90 && line.x0 == 5 && line.x1 == 94 && line.y0 == 14 && line.y1 == 14);
        CHECK(same(px(h, 50, 14), c.border_subtle));
        SoftwareSurface v(100, 50);
        { Painter p(v); paint_separator(p, t, {40, 4, 1, 40}, Orientation::Vertical); }
        const Ink vline = ink_over(v, {0, 0, 100, 50}, Color{0, 0, 0, 0});
        CHECK(vline.pixels == 40 && vline.x0 == 40 && vline.x1 == 40 && vline.y0 == 4 && vline.y1 == 43);
        SoftwareSurface empty(100, 50);
        { Painter p(empty); paint_separator(p, t, {5, 10, 0, 9}, Orientation::Horizontal); paint_separator(p, t, {5, 10, 90, 0}, Orientation::Vertical); }
        CHECK(ink_over(empty, {0, 0, 100, 50}, Color{0, 0, 0, 0}).pixels == 0);
    }
    vynex_test::context().clear();
    std::printf("test_selection_indicator_and_separator_pixels passed\n");
}

// ---------------------------------------------------------------------------------------------------------
// The geometry invariant: a border never encloses pixels its fill does not cover
// ---------------------------------------------------------------------------------------------------------
void test_fill_and_border_stay_aligned_at_every_size() {
    // Each component is painted over a magenta backdrop at many sizes (as a control that is being resized would be).
    // Wherever the component's own shape fully covers a pixel, none of the backdrop may remain: an outline that had
    // grown past its fill would leave magenta inside it. The reference shape is the component's rectangle and radii.
    const Color magenta{255, 0, 255, 255};
    const Theme& t = default_theme();
    struct Kind {
        const char* name;
        BoxStyle style;
    };
    const Kind kinds[] = {
        {"default button", button_style(t, ButtonVariant::Default, kIdle)},
        {"hovered button", button_style(t, ButtonVariant::Default, kHovered)},
        {"primary button", button_style(t, ButtonVariant::Primary, kIdle)},
        {"disabled button", button_style(t, ButtonVariant::Default, kDisabled)},
        {"sunken panel", panel_style(t, PanelVariant::Sunken)},
        {"raised panel", panel_style(t, PanelVariant::Raised)},
        {"overlay panel", panel_style(t, PanelVariant::Overlay)},
        {"surface panel", panel_style(t, PanelVariant::Surface)},
    };
    size_t checked = 0;
    for (const Kind& kind : kinds) {
        for (uint32_t w = 1; w <= 72; w += (w < 24 ? 1 : 7)) {
            for (uint32_t h = 1; h <= 56; h += (h < 24 ? 1 : 9)) {
                vynex_test::context() = std::string(kind.name) + " " + std::to_string(w) + "x" + std::to_string(h);
                const Rect rect{6, 5, w, h};
                SoftwareSurface shape(90, 70), s(90, 70);
                { Painter p(shape); p.fill_rounded_rect(rect, kind.style.radii, Color{255, 255, 255, 255}); }
                fill_all(s, magenta);
                { Painter p(s); paint_box(p, rect, kind.style); }
                auto cover = shape.map_pixels();
                auto out = s.map_pixels();
                for (size_t i = 0; i < out.size(); i += 4) {
                    const uint8_t c = cover[i + 3];
                    const bool is_magenta = out[i] == 255 && out[i + 1] == 0 && out[i + 2] == 255;
                    if (c == 0) CHECK(is_magenta);        // nothing drawn outside the shape
                    if (c == 255) {
                        CHECK(!is_magenta);               // and no hole inside it
                        CHECK(out[i + 1] != 0 || out[i] != 255); // (not even a trace of the backdrop's hue)
                    }
                }
                shape.unmap_pixels();
                s.unmap_pixels();
                ++checked;
            }
        }
    }
    CHECK(checked > 3000);
    vynex_test::context().clear();
    std::printf("test_fill_and_border_stay_aligned_at_every_size passed (%zu boxes)\n", checked);
}

// ---------------------------------------------------------------------------------------------------------
// Clipping and edge conditions
// ---------------------------------------------------------------------------------------------------------
void test_clipping_and_edge_conditions() {
    const Theme& t = default_theme();
    const Rect rect{20, 14, 110, 32};

    // Clipped painting equals unclipped painting inside the clip, and touches nothing outside it (ring included).
    SoftwareSurface whole(160, 64);
    { Painter p(whole); paint_button(p, t, text(), rect, "Clipped label", ButtonVariant::Default, kFocused); }
    const auto reference = snapshot(whole);
    for (const Rect& clip : {Rect{0, 0, 160, 64}, Rect{30, 20, 40, 12}, Rect{16, 10, 10, 10}, Rect{100, 30, 60, 34}, Rect{60, 0, 1, 64}}) {
        SoftwareSurface s(160, 64);
        {
            Painter p(s);
            const Painter::ClipScope scope(p, clip);
            paint_button(p, t, text(), rect, "Clipped label", ButtonVariant::Default, kFocused);
        }
        const auto clipped = snapshot(s);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 160; ++x) {
                const bool in_clip = x >= clip.x && x < clip.x + static_cast<int>(clip.width) && y >= clip.y && y < clip.y + static_cast<int>(clip.height);
                const size_t i = (static_cast<size_t>(y) * 160 + static_cast<size_t>(x)) * 4;
                for (size_t ch = 0; ch < 4; ++ch) CHECK(clipped[i + ch] == (in_clip ? reference[i + ch] : 0));
            }
        }
    }
    // The painter's clip is exactly what it was afterwards (components push and pop in balance).
    {
        SoftwareSurface s(160, 64);
        Painter p(s);
        CHECK(p.push_clip({5, 5, 100, 40}));
        const Rect before = p.clip_bounds();
        paint_button(p, t, text(), rect, "Label", ButtonVariant::Primary, kFocused);
        paint_icon_button(p, t, {10, 10, 28, 28}, IconId::Close, kHovered);
        paint_list_row(p, t, text(), rect, "Row", kSelected);
        const Rect after = p.clip_bounds();
        CHECK(before.x == after.x && before.y == after.y && before.width == after.width && before.height == after.height);
    }

    // Empty rectangles paint nothing at all (no label, no ring, no keyline).
    SoftwareSurface nothing(160, 64);
    {
        Painter p(nothing);
        for (const Rect& r : {Rect{20, 14, 0, 32}, Rect{20, 14, 110, 0}, Rect{20, 14, 0, 0}}) {
            paint_button(p, t, text(), r, "Label", ButtonVariant::Default, with(true, true, true, false, true));
            paint_icon_button(p, t, r, IconId::Close, kFocused);
            paint_panel(p, t, r, PanelVariant::Overlay);
            paint_list_row(p, t, text(), r, "Row", with(false, false, true, false, true));
            paint_selection_indicator(p, t, r, IndicatorEdge::Leading);
            paint_separator(p, t, r, Orientation::Horizontal);
            paint_focus_ring(p, t, r, CornerRadii::uniform(6.0f));
        }
    }
    CHECK(ink_over(nothing, {0, 0, 160, 64}, Color{0, 0, 0, 0}).pixels == 0);

    // Without a text renderer, and with an empty label, the box is drawn and the label is simply absent.
    SoftwareSurface no_text(160, 64), no_label(160, 64), box_only(160, 64);
    { Painter p(no_text); paint_button(p, t, nullptr, rect, "Label", ButtonVariant::Default, kIdle); }
    { Painter p(no_label); paint_button(p, t, text(), rect, "", ButtonVariant::Default, kIdle); }
    { Painter p(box_only); paint_box(p, rect, button_style(t, ButtonVariant::Default, kIdle)); }
    CHECK(snapshot(no_text) == snapshot(box_only) && snapshot(no_label) == snapshot(box_only));

    // A label that fits only without the preferred padding keeps its text (the Explorer's 40 px "Up" button); one that
    // cannot fit at all is shortened and still never leaves the button.
    const BoxStyle style = button_style(t, ButtonVariant::Default, kIdle);
    {
        const Rect small{20, 14, 40, 30};
        SoftwareSurface s(100, 60);
        { Painter p(s); paint_button(p, t, text(), small, "Up", ButtonVariant::Default, kIdle); }
        const Ink label = ink_over(s, label_area(small), style.fill);
        const float width = text()->measure("Up", t.type.body_strong).width;
        CHECK(width > static_cast<float>(small.width) - 2.0f * static_cast<float>(t.spacing.md)); // it really does not fit the padding
        CHECK(label.any() && (label.x1 - label.x0 + 1) >= static_cast<int>(width) - 3);             // drawn whole, not as an ellipsis
        CHECK(label.x0 >= small.x + t.spacing.xs && label.x1 < small.x + static_cast<int>(small.width) - t.spacing.xs);
        CHECK(std::fabs((label.x0 + label.x1 + 1) / 2.0 - (small.x + small.width / 2.0)) <= 2.0);
    }
    {
        const Rect narrow{20, 14, 60, 32};
        SoftwareSurface s(120, 60);
        { Painter p(s); paint_button(p, t, text(), narrow, "A label far too long for this button", ButtonVariant::Default, kIdle); }
        const Ink label = ink_over(s, label_area(narrow), style.fill);
        CHECK(label.any() && label.x0 >= narrow.x + t.spacing.xs && label.x1 < narrow.x + static_cast<int>(narrow.width) - t.spacing.xs);
        CHECK(ink_over(s, {0, 0, 120, 60}, Color{0, 0, 0, 0}).x1 < narrow.x + static_cast<int>(narrow.width)); // nothing right of the button
    }
    // Text that is not valid UTF-8 is drawn as replacement boxes inside the button, not as an error.
    {
        SoftwareSurface s(160, 64);
        { Painter p(s); paint_button(p, t, text(), rect, std::string_view("\xFF\xFE ok \xC3", 7), ButtonVariant::Default, kIdle); }
        CHECK(ink_over(s, label_area(rect), style.fill).any());
        CHECK(ink_over(s, {0, 0, 160, 64}, Color{0, 0, 0, 0}).x1 < rect.x + static_cast<int>(rect.width));
    }
    std::printf("test_clipping_and_edge_conditions passed\n");
}

// A surface with guard bytes on both sides of its pixels.
class GuardedSurface final : public ISurface {
public:
    static constexpr size_t kGuard = 256;
    GuardedSurface(uint32_t w, uint32_t h) : m_w(w), m_h(h), m_store(kGuard * 2 + static_cast<size_t>(w) * h * 4, 0xAB) {
        std::fill(m_store.begin() + kGuard, m_store.end() - kGuard, uint8_t{0});
    }
    uint32_t width() const override { return m_w; }
    uint32_t height() const override { return m_h; }
    std::span<uint8_t> map_pixels() override { return std::span<uint8_t>(m_store.data() + kGuard, static_cast<size_t>(m_w) * m_h * 4); }
    void unmap_pixels() override {}
    [[nodiscard]] bool guards_intact() const {
        for (size_t i = 0; i < kGuard; ++i) {
            if (m_store[i] != 0xAB || m_store[m_store.size() - 1 - i] != 0xAB) return false;
        }
        return true;
    }

private:
    uint32_t m_w, m_h;
    std::vector<uint8_t> m_store;
};

void test_bounds_safety_fuzz() {
    struct Rng {
        uint64_t s;
        uint32_t next() {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<uint32_t>(s >> 33);
        }
        int32_t coord() {
            switch (next() % 10) {
            case 0: return std::numeric_limits<int32_t>::min();
            case 1: return std::numeric_limits<int32_t>::max();
            default: return static_cast<int32_t>(next() % 500) - 250;
            }
        }
        uint32_t dim() {
            switch (next() % 10) {
            case 0: return 0;
            case 1: return std::numeric_limits<uint32_t>::max();
            case 2: return 0x7FFFFFFFu;
            default: return next() % 300;
            }
        }
    };
    const std::string_view labels[] = {"", "OK", "A much longer label than any button here", std::string_view("\xF0\x9F\x98\x80 \xFF", 6)};
    uint64_t seed = 0xC0DE;
    for (const auto& [w, h] : {std::pair<uint32_t, uint32_t>{96, 64}, {1, 1}, {7, 200}, {200, 7}}) {
        for (const ThemeId id : kThemes) {
            GuardedSurface s(w, h);
            Rng rng{seed++};
            Painter p(s);
            const Theme& t = theme_for(id);
            for (int i = 0; i < 2500; ++i) {
                const Rect r{rng.coord(), rng.coord(), rng.dim(), rng.dim()};
                const ControlState state = with((rng.next() & 1) != 0, (rng.next() & 1) != 0, (rng.next() & 1) != 0, (rng.next() & 3) == 0, (rng.next() & 1) != 0);
                const std::string_view label = labels[rng.next() % 4];
                switch (rng.next() % 9) {
                case 0: paint_button(p, t, text(), r, label, static_cast<ButtonVariant>(rng.next() % 3), state); break;
                case 1: paint_icon_button(p, t, r, static_cast<IconId>(rng.next() % 4), state); break;
                case 2: paint_panel(p, t, r, static_cast<PanelVariant>(rng.next() % 4)); break;
                case 3: paint_list_row(p, t, text(), r, label, state); break;
                case 4: paint_selection_indicator(p, t, r, (rng.next() & 1) != 0 ? IndicatorEdge::Leading : IndicatorEdge::Bottom, state); break;
                case 5: paint_separator(p, t, r, (rng.next() & 1) != 0 ? Orientation::Horizontal : Orientation::Vertical); break;
                case 6: paint_focus_ring(p, t, r, CornerRadii::uniform(static_cast<float>(rng.next() % 40))); break;
                case 7: p.push_clip(r); break;
                default: p.pop_clip(); break;
                }
                if (!s.guards_intact()) {
                    std::printf("  guard bytes corrupted at iteration %d\n", i);
                    CHECK(false);
                }
            }
        }
    }
    std::printf("test_bounds_safety_fuzz passed\n");
}

// ---------------------------------------------------------------------------------------------------------
// The toolkit's legacy entry points
// ---------------------------------------------------------------------------------------------------------
void test_toolkit_legacy_widgets_use_the_components() {
    for (const ThemeId id : kThemes) {
        vynex_test::context() = name_of(id);
        const Theme& t = theme_for(id);
        BasicWidgetToolkit toolkit(text(), t);
        CHECK(&toolkit.theme() == &t && toolkit.text_renderer() == text());

        // draw_button is the default button at rest; draw_panel is the surface panel. Pixel for pixel.
        auto legacy = std::make_shared<SoftwareSurface>(200, 80);
        toolkit.draw_button(legacy, 20, 20, 120, 40, "Click Me");
        SoftwareSurface direct(200, 80);
        { Painter p(direct); paint_button(p, t, text(), {20, 20, 120, 40}, "Click Me", ButtonVariant::Default, kIdle); }
        CHECK(snapshot(*legacy) == snapshot(direct));

        auto legacy_panel = std::make_shared<SoftwareSurface>(200, 80);
        toolkit.draw_panel(legacy_panel, 5, 6, 150, 60);
        SoftwareSurface direct_panel(200, 80);
        { Painter p(direct_panel); paint_panel(p, t, {5, 6, 150, 60}, PanelVariant::Surface); }
        CHECK(snapshot(*legacy_panel) == snapshot(direct_panel));
        CHECK(same(px(*legacy_panel, 80, 30), t.colors.surface));

        // Null surfaces are ignored, as before.
        toolkit.draw_button(nullptr, 0, 0, 10, 10, "x");
        toolkit.draw_panel(nullptr, 0, 0, 10, 10);
    }
    vynex_test::context().clear();

    // The default toolkit is the dark theme with the shared renderer.
    BasicWidgetToolkit default_toolkit;
    CHECK(&default_toolkit.theme() == &default_theme());
    CHECK(default_toolkit.text_renderer() == TextRenderer::shared_default());

    // Without a font engine the button is the same box, labelled with the 8x8 bitmap font in text_primary.
    BasicWidgetToolkit bitmap(nullptr);
    CHECK(bitmap.text_renderer() == nullptr);
    auto s = std::make_shared<SoftwareSurface>(200, 80);
    bitmap.draw_button(s, 20, 20, 120, 40, "OK");
    const BoxStyle style = button_style(default_theme(), ButtonVariant::Default, kIdle);
    CHECK(same(px(*s, 100, 22), style.fill) && same(px(*s, 100, 20), style.border));
    const Rect area = label_area({20, 20, 120, 40});
    const Ink label = ink_over(*s, area, style.fill);
    CHECK(label.any() && label.x0 >= 20 + default_theme().spacing.md && label.x1 < 20 + default_theme().spacing.md + 16); // two 8 px cells
    size_t exact = 0;
    for (int y = area.y; y < area.y + static_cast<int>(area.height); ++y) {
        for (int x = area.x; x < area.x + static_cast<int>(area.width); ++x) exact += same(px(*s, x, y), default_theme().colors.text_primary);
    }
    CHECK(exact == label.pixels && exact > 10); // 1-bit glyphs: every label pixel is exactly the text colour

    // An interface without overrides still answers (the defaults): dark theme, no renderer.
    struct Minimal final : IWidgetToolkit {
        void draw_rect(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t, Color) override {}
        void draw_text(std::shared_ptr<ISurface>, int32_t, int32_t, std::string_view, Color) override {}
        void draw_button(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t, std::string_view) override {}
        void draw_panel(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t) override {}
    } minimal;
    CHECK(&minimal.theme() == &default_theme() && minimal.text_renderer() == nullptr);
    std::printf("test_toolkit_legacy_widgets_use_the_components passed\n");
}

// ---------------------------------------------------------------------------------------------------------
// No heap allocation while painting
// ---------------------------------------------------------------------------------------------------------
void test_no_heap_allocation_while_painting() {
    SoftwareSurface s(320, 200);
    const Theme& dark = theme_for(ThemeId::Dark);
    const Theme& light = theme_for(ThemeId::Light);
    const TextRenderer* renderer = text();
    auto paint_everything = [&](const Theme& t) {
        Painter p(s);
        const Painter::ClipScope scope(p, {4, 4, 300, 180});
        paint_panel(p, t, {0, 0, 320, 200}, PanelVariant::Surface);
        paint_panel(p, t, {10, 10, 140, 90}, PanelVariant::Sunken);
        paint_panel(p, t, {160, 10, 140, 90}, PanelVariant::Raised);
        paint_panel(p, t, {160, 110, 140, 80}, PanelVariant::Overlay);
        paint_button(p, t, renderer, {20, 20, 100, 32}, "Default", ButtonVariant::Default, kHovered);
        paint_button(p, t, renderer, {20, 60, 100, 32}, "A primary button with a long label", ButtonVariant::Primary, kFocused);
        paint_button(p, t, renderer, {170, 20, 100, 32}, "Delete", ButtonVariant::Destructive, kPressed);
        paint_button(p, t, renderer, {170, 60, 100, 32}, "Off", ButtonVariant::Default, kDisabled);
        paint_icon_button(p, t, {20, 110, 28, 28}, IconId::Close, kSelected);
        paint_icon_button(p, t, {56, 110, 28, 28}, IconId::Maximize, kFocused);
        paint_list_row(p, t, renderer, {10, 150, 140, 32}, "Documents", with(true, false, true, false, true));
        paint_list_row(p, t, renderer, {170, 120, 120, 28}, "notes.md", kIdle);
        paint_selection_indicator(p, t, {170, 150, 120, 32}, IndicatorEdge::Bottom);
        paint_separator(p, t, {10, 104, 300, 1}, Orientation::Horizontal);
        paint_separator(p, t, {154, 10, 1, 180}, Orientation::Vertical);
        paint_focus_ring(p, t, {200, 150, 60, 24}, CornerRadii::uniform(6.0f));
        (void)button_style(t, ButtonVariant::Default, kHovered);
        (void)selection_indicator_rect(t, {0, 0, 100, 32}, IndicatorEdge::Leading);
    };
    paint_everything(dark); // warm up anything the runtime allocates lazily

    // Prove the counter works, or "0 allocations" would mean nothing.
    g_alloc_count = 0;
    g_count_allocs = true;
    {
        auto probe = std::make_unique<std::vector<int>>(16);
        CHECK(!probe->empty());
    }
    g_count_allocs = false;
    CHECK(g_alloc_count >= 2);

    g_alloc_count = 0;
    g_count_allocs = true;
    for (int i = 0; i < 25; ++i) {
        paint_everything(dark);
        paint_everything(light);
    }
    g_count_allocs = false;
    if (g_alloc_count != 0) std::printf("  unexpected heap allocations: %zu\n", g_alloc_count.load());
    CHECK(g_alloc_count == 0);
    std::printf("test_no_heap_allocation_while_painting passed\n");
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("Running component tests...\n");
    test_state_model();
    test_button_styles();
    test_destructive_button_in_every_state();
    test_icon_button_panel_and_row_styles();
    test_geometry_helpers();
    test_button_pixels_in_every_state();
    test_icon_button_pixels();
    test_panel_pixels();
    test_list_row_pixels();
    test_selection_indicator_and_separator_pixels();
    test_fill_and_border_stay_aligned_at_every_size();
    test_clipping_and_edge_conditions();
    test_bounds_safety_fuzz();
    test_toolkit_legacy_widgets_use_the_components();
    test_no_heap_allocation_while_painting();
    std::printf("All component tests passed.\n");
    return 0;
}
