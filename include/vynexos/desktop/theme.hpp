#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/paint_types.hpp"

namespace vynexos::desktop {

// -------------------------------------------------------------------------------------------------
// Colour helpers
// -------------------------------------------------------------------------------------------------

/// 0xRRGGBB (+ optional alpha) -> Color.
[[nodiscard]] constexpr Color rgb(uint32_t hex, uint8_t alpha = 255) noexcept {
    return Color{static_cast<uint8_t>((hex >> 16) & 0xFFu), static_cast<uint8_t>((hex >> 8) & 0xFFu),
                 static_cast<uint8_t>(hex & 0xFFu), alpha};
}

[[nodiscard]] constexpr Color with_alpha(Color c, uint8_t alpha) noexcept {
    return Color{c.r, c.g, c.b, alpha};
}

/// Straight per-channel interpolation (t in [0,1]).
[[nodiscard]] constexpr Color mix(Color a, Color b, float t) noexcept {
    const float u = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    auto ch = [u](uint8_t x, uint8_t y) {
        return static_cast<uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * u + 0.5f);
    };
    return Color{ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
}

/// Straight-alpha "source over": `top` composited over `bottom`. This is how a translucent state overlay (hover,
/// pressed, selection tint) and the surface under it become the one colour a component is filled with.
[[nodiscard]] constexpr Color blend_over(Color top, Color bottom) noexcept {
    const float ta = static_cast<float>(top.a) / 255.0f;
    const float ba = static_cast<float>(bottom.a) / 255.0f;
    const float out_a = ta + ba * (1.0f - ta);
    if (out_a <= 0.0f) return Color{0, 0, 0, 0};
    auto ch = [=](uint8_t t, uint8_t b) {
        const float v = (static_cast<float>(t) * ta + static_cast<float>(b) * ba * (1.0f - ta)) / out_a;
        return static_cast<uint8_t>(v + 0.5f);
    };
    return Color{ch(top.r, bottom.r), ch(top.g, bottom.g), ch(top.b, bottom.b), static_cast<uint8_t>(out_a * 255.0f + 0.5f)};
}

/// WCAG relative luminance of an sRGB colour (alpha ignored).
[[nodiscard]] inline double relative_luminance(Color c) noexcept {
    auto lin = [](uint8_t v) {
        const double s = static_cast<double>(v) / 255.0;
        return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

/// WCAG contrast ratio between two opaque colours (1.0 .. 21.0).
[[nodiscard]] inline double contrast_ratio(Color a, Color b) noexcept {
    const double la = relative_luminance(a);
    const double lb = relative_luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// -------------------------------------------------------------------------------------------------
// Design tokens
//
// The single source of truth for VynexOS visuals ("Signal": graphite surfaces, one cyan accent reserved for focus,
// selection and the active item). Components must read from a Theme instead of embedding colours or dimensions.
//
// ColorTokens' default member values ARE the dark theme; the light theme assigns every token in
// light_color_tokens(). for_each_color_token() names every token once, so tests can prove both palettes complete.
// -------------------------------------------------------------------------------------------------

struct ColorTokens {
    // Surfaces, darkest to lightest (in the dark theme).
    Color bg_deep{rgb(0x0E1216)};          // wallpaper base, taskbar base, shadow base
    Color bg{rgb(0x12171C)};               // desktop
    Color surface_sunken{rgb(0x151A20)};   // inputs, list wells, terminal body
    Color surface{rgb(0x1A2027)};          // window body, panels
    Color surface_raised{rgb(0x222A33)};   // focused title bar, cards, default buttons
    Color surface_overlay{rgb(0x2A333D)};  // launcher, menus, popovers, notifications

    // Borders.
    Color border_subtle{rgb(0x28313A)};    // separators, inactive window border
    Color border{rgb(0x36414D)};           // panel and overlay borders
    Color border_strong{rgb(0x4A5663)};    // emphasised dividers, scrollbar thumb
    Color control_border{rgb(0x66727F)};   // input, checkbox and outlined-button edges (>= 3:1 against surfaces)

    // Text.
    Color text_primary{rgb(0xE8EEF4)};
    Color text_secondary{rgb(0x9AA7B4)};
    Color text_disabled{rgb(0x5C6773)};
    Color text_on_accent{rgb(0x04222A)};   // on an accent fill
    Color text_on_danger{rgb(0xFFFFFF)};   // on a danger_solid fill

    // Accent (cyan) and its interaction states.
    Color accent{rgb(0x22C7DC)};               // keyline, primary button, links
    Color accent_hover{rgb(0x4BD8EA)};
    Color accent_pressed{rgb(0x159FB2)};
    Color accent_subtle{rgb(0x22C7DC, 0x26)};  // selected row / active item fill (15%)
    Color focus_ring{rgb(0x22C7DC, 0xB0)};     // 2 px keyboard focus ring (69%)
    Color selection_text{rgb(0x22C7DC, 0x59)}; // text selection (35%)

    // Status.
    Color danger{rgb(0xE5484D)};           // error icons, close-button hover
    Color danger_hover{rgb(0xF2666B)};
    Color danger_pressed{rgb(0xC13338)};
    // Destructive button fill. CORRECTED from the Stage 7.0 table's #D03A40: with the 7% white hover overlay
    // composited over it (#D3484D) the white label measured 4.37:1, below AA. #CA383E (the approved value with each
    // channel scaled by 0.97) gives 5.06:1 at rest, 4.56:1 hovered; pressed uses danger_pressed (5.53:1).
    Color danger_solid{rgb(0xCA383E)};
    Color danger_text{rgb(0xF47A7E)};      // error text
    Color success{rgb(0x3DD68C)};
    Color warning{rgb(0xF5A524)};
    Color info{rgb(0x5AA9F5)};

    // Interaction overlays, composited over any surface.
    Color hover_overlay{rgb(0xFFFFFF, 0x12)};    // 7%
    Color pressed_overlay{rgb(0xFFFFFF, 0x1E)};  // 12%
    Color scrim{rgb(0x000000, 0x80)};            // behind modal dialogs (50%)
};

/// Number of colour tokens. A token added to ColorTokens without being listed in for_each_color_token() (and so
/// without a light value being checked) fails the static_assert below.
inline constexpr std::size_t kColorTokenCount = 32;
static_assert(sizeof(ColorTokens) == kColorTokenCount * sizeof(Color), "list every ColorTokens member in for_each_color_token()");

/// Calls fn(name, colour) for every colour token, in declaration order.
template <class Fn>
constexpr void for_each_color_token(const ColorTokens& c, Fn&& fn) {
    fn("bg_deep", c.bg_deep);
    fn("bg", c.bg);
    fn("surface_sunken", c.surface_sunken);
    fn("surface", c.surface);
    fn("surface_raised", c.surface_raised);
    fn("surface_overlay", c.surface_overlay);
    fn("border_subtle", c.border_subtle);
    fn("border", c.border);
    fn("border_strong", c.border_strong);
    fn("control_border", c.control_border);
    fn("text_primary", c.text_primary);
    fn("text_secondary", c.text_secondary);
    fn("text_disabled", c.text_disabled);
    fn("text_on_accent", c.text_on_accent);
    fn("text_on_danger", c.text_on_danger);
    fn("accent", c.accent);
    fn("accent_hover", c.accent_hover);
    fn("accent_pressed", c.accent_pressed);
    fn("accent_subtle", c.accent_subtle);
    fn("focus_ring", c.focus_ring);
    fn("selection_text", c.selection_text);
    fn("danger", c.danger);
    fn("danger_hover", c.danger_hover);
    fn("danger_pressed", c.danger_pressed);
    fn("danger_solid", c.danger_solid);
    fn("danger_text", c.danger_text);
    fn("success", c.success);
    fn("warning", c.warning);
    fn("info", c.info);
    fn("hover_overlay", c.hover_overlay);
    fn("pressed_overlay", c.pressed_overlay);
    fn("scrim", c.scrim);
}

/// The light palette: every token assigned (nothing inherits a dark value). Elevation climbs towards white, the accent
/// is a darker cyan that carries text contrast on light surfaces, and hover/pressed darken instead of lighten.
[[nodiscard]] constexpr ColorTokens light_color_tokens() noexcept {
    ColorTokens c;
    c.bg_deep = rgb(0xC9D1D9);
    c.bg = rgb(0xDCE2E8);
    c.surface_sunken = rgb(0xEEF1F4);
    c.surface = rgb(0xF6F8FA);
    c.surface_raised = rgb(0xFCFDFE);
    c.surface_overlay = rgb(0xFFFFFF);

    c.border_subtle = rgb(0xE3E8ED);
    c.border = rgb(0xCDD5DD);
    c.border_strong = rgb(0xA9B4BF);
    c.control_border = rgb(0x7D8996);

    c.text_primary = rgb(0x111820);
    c.text_secondary = rgb(0x4A5764);
    c.text_disabled = rgb(0x98A3AE);
    c.text_on_accent = rgb(0xFFFFFF);
    c.text_on_danger = rgb(0xFFFFFF);

    c.accent = rgb(0x087482);
    c.accent_hover = rgb(0x076976);
    c.accent_pressed = rgb(0x055963);
    c.accent_subtle = rgb(0x087482, 0x1F);   // 12%
    c.focus_ring = rgb(0x087482, 0xCC);      // 80%
    c.selection_text = rgb(0x087482, 0x40);  // 25%

    c.danger = rgb(0xC8323A);
    // danger_hover and danger_pressed are DERIVED, not values from the Stage 7.0 table (which lists no light variants):
    // each channel of the light `danger` (#C8323A) scaled by 0.9 and 0.8 and rounded, i.e. darkened 10% and 20%, the
    // direction hover and pressed take on a light surface. Contrast: white text on them 6.24:1 and 7.40:1; as icons on
    // every light surface 4.78 and 5.67 at the lowest. (No component reads light danger_hover today; the pressed
    // destructive button and the pressed close button read danger_pressed.)
    c.danger_hover = rgb(0xB42D34);
    c.danger_pressed = rgb(0xA0282E);
    c.danger_solid = rgb(0xC8323A);
    c.danger_text = rgb(0xB4262E);
    c.success = rgb(0x137547);
    c.warning = rgb(0x955800);
    c.info = rgb(0x1F62C4);

    c.hover_overlay = rgb(0x000000, 0x0D);    // 5%
    c.pressed_overlay = rgb(0x000000, 0x17);  // 9%
    c.scrim = rgb(0x000000, 0x66);            // 40%
    return c;
}

/// 4px-based spacing scale (pixels).
struct SpacingTokens {
    int32_t xxs{2};
    int32_t xs{4};
    int32_t sm{8};
    int32_t md{12};
    int32_t lg{16};
    int32_t xl{24};
    int32_t xxl{32};
    int32_t xxxl{48};
};

/// Corner radii (pixels). Components use four of them, chosen by role: `sm` (4: checkboxes, title-bar controls),
/// `control` (6: buttons, inputs, rows, tabs, tooltips), `md` (8: menus, popovers, notifications, cards) and
/// `window` (10: windows, launcher, dialogs). A nested shape's radius is its container's radius minus the padding.
struct RadiusTokens {
    float none{0.0f};
    float xs{2.0f};
    float sm{4.0f};
    float control{6.0f};
    float md{8.0f};
    float lg{12.0f};
    float xl{16.0f};
    float window{10.0f};
};

/// Layout dimensions (pixels, in the logical 1920x1080 desktop space).
struct SizeTokens {
    int32_t border_width{1};          // every component border and separator
    int32_t window_border{1};
    int32_t titlebar_height{36};
    int32_t control_button{28};
    int32_t control_button_gap{4};
    int32_t resize_grip{6};
    int32_t taskbar_height{48};
    int32_t taskbar_button_height{36};
    int32_t taskbar_button_min_width{132};
    int32_t launcher_width{420};
    int32_t launcher_height{520};
    int32_t launcher_item{96};
    int32_t search_field_height{40};
    int32_t icon_sm{16};
    int32_t icon_md{20};
    int32_t icon_24{24};              // taskbar and toolbar icons (between icon_md and icon_lg)
    int32_t icon_lg{32};
    int32_t icon_xl{48};
    int32_t control_height_sm{24};
    int32_t control_height{32};       // buttons, inputs
    int32_t control_height_lg{40};    // search fields
    int32_t icon_button{28};
    int32_t row_height_compact{28};
    int32_t row_height{32};
    int32_t selection_indicator{2};   // the keyline that marks the selected / active item
    int32_t focus_ring_width{2};
    int32_t focus_ring_offset{2};     // gap between a control and its focus ring
    int32_t min_hit_target{28};
};

enum class FontWeight : uint16_t { Regular = 400, Medium = 500, SemiBold = 600, Bold = 700 };

/// One text style. The text renderer builds a glyph strike for each distinct size/weight among the typography tokens.
/// `letter_spacing_px` is not applied by the renderer (text advances by the font's designed widths).
struct TextStyle {
    float size_px{14.0f};
    float line_height_px{20.0f};
    FontWeight weight{FontWeight::Regular};
    float letter_spacing_px{0.0f};
};

struct TypographyTokens {
    TextStyle caption{12.0f, 16.0f, FontWeight::Regular, 0.0f};   // status bars, helper text
    TextStyle metadata{12.0f, 16.0f, FontWeight::Medium, 0.0f};   // dates, sizes, counts (in text_secondary)
    TextStyle body{14.0f, 20.0f, FontWeight::Regular, 0.0f};
    TextStyle body_strong{14.0f, 20.0f, FontWeight::Medium, 0.0f};
    TextStyle title{15.0f, 22.0f, FontWeight::SemiBold, 0.0f};
    TextStyle heading{20.0f, 28.0f, FontWeight::SemiBold, 0.0f};
    TextStyle display{32.0f, 40.0f, FontWeight::SemiBold, 0.0f};
};

/// Restrained depth. Peak opacities are deliberately low so shadows read as separation, not decoration.
struct ElevationTokens {
    ShadowSpec control{8.0f, 0.0f, 2.0f, 0.0f, Color{0, 0, 0, 0x40}};
    ShadowSpec window_inactive{18.0f, 0.0f, 6.0f, 0.0f, Color{0, 0, 0, 0x48}};
    ShadowSpec window_active{28.0f, 0.0f, 10.0f, 0.0f, Color{0, 0, 0, 0x70}};
    ShadowSpec popup{32.0f, 0.0f, 12.0f, 0.0f, Color{0, 0, 0, 0x80}};
};

struct Theme {
    ColorTokens colors{};
    SpacingTokens spacing{};
    RadiusTokens radius{};
    SizeTokens size{};
    TypographyTokens type{};
    ElevationTokens elevation{};
};

/// The built-in VynexOS dark graphite theme.
inline constexpr Theme kVynexDarkTheme{};

/// The light counterpart: the same spacing, radii, sizes, typography and elevation with the light palette.
[[nodiscard]] constexpr Theme make_light_theme() noexcept {
    Theme t;
    t.colors = light_color_tokens();
    return t;
}
inline constexpr Theme kVynexLightTheme = make_light_theme();

enum class ThemeId : uint8_t { Dark, Light };

/// The built-in theme for an id. Selection is a pure function of the id: no global state, no environment.
[[nodiscard]] constexpr const Theme& theme_for(ThemeId id) noexcept {
    return id == ThemeId::Light ? kVynexLightTheme : kVynexDarkTheme;
}

/// The default theme is dark. (There is no runtime theme switch yet; a component is given its theme explicitly.)
[[nodiscard]] inline const Theme& default_theme() noexcept { return kVynexDarkTheme; }

/// Bridges the token theme to the legacy five-colour WindowTheme used by the current decorator, so existing
/// call sites can adopt tokens incrementally.
[[nodiscard]] constexpr WindowTheme to_window_theme(const Theme& t) noexcept {
    return WindowTheme{t.colors.surface_overlay, t.colors.surface_raised, t.colors.border, t.colors.surface,
                       t.colors.text_primary};
}

} // namespace vynexos::desktop
