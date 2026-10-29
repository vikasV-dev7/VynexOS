#pragma once
#include <algorithm>
#include <cmath>
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
// The single source of truth for VynexOS visuals. Components must read from a Theme instead of embedding
// colours or dimensions. Dark graphite surfaces with one restrained cyan accent.
// -------------------------------------------------------------------------------------------------

struct ColorTokens {
    // Surfaces, darkest to lightest.
    Color bg_deep{rgb(0x0E1216)};          // behind everything / shadow base
    Color bg{rgb(0x12171C)};               // desktop base
    Color surface{rgb(0x1A2027)};          // window body, panels
    Color surface_raised{rgb(0x222A33)};   // title bars, cards, inactive controls
    Color surface_overlay{rgb(0x2A333D)};  // popovers, menus, hovered rows

    // Borders.
    Color border_subtle{rgb(0x28313A)};
    Color border{rgb(0x36414D)};
    Color border_strong{rgb(0x4A5663)};

    // Text.
    Color text_primary{rgb(0xE8EEF4)};
    Color text_secondary{rgb(0x9AA7B4)};
    Color text_disabled{rgb(0x5C6773)};
    Color text_on_accent{rgb(0x04222A)};

    // Accent (cyan) and its interaction states.
    Color accent{rgb(0x22C7DC)};
    Color accent_hover{rgb(0x4BD8EA)};
    Color accent_pressed{rgb(0x159FB2)};
    Color accent_subtle{rgb(0x22C7DC, 0x26)};  // selection / active background tint
    Color focus_ring{rgb(0x22C7DC, 0xB0)};

    // Status.
    Color danger{rgb(0xE5484D)};
    Color danger_hover{rgb(0xF2666B)};
    Color danger_pressed{rgb(0xC13338)};
    Color success{rgb(0x3DD68C)};
    Color warning{rgb(0xF5A524)};

    // Interaction overlays, composited over any surface.
    Color hover_overlay{rgb(0xFFFFFF, 0x12)};
    Color pressed_overlay{rgb(0xFFFFFF, 0x1E)};
    Color scrim{rgb(0x000000, 0x80)};
};

/// 4px-based spacing scale (pixels).
struct SpacingTokens {
    int32_t xxs{2};
    int32_t xs{4};
    int32_t sm{8};
    int32_t md{12};
    int32_t lg{16};
    int32_t xl{24};
    int32_t xxl{32};
};

/// Corner radii (pixels).
struct RadiusTokens {
    float none{0.0f};
    float xs{2.0f};
    float sm{4.0f};
    float md{8.0f};
    float lg{12.0f};
    float xl{16.0f};
    float window{10.0f};
};

/// Layout dimensions (pixels, in the logical 1920x1080 desktop space).
struct SizeTokens {
    int32_t window_border{1};
    int32_t titlebar_height{36};
    int32_t control_button{28};
    int32_t control_button_gap{4};
    int32_t resize_grip{6};
    int32_t taskbar_height{48};
    int32_t taskbar_button_height{36};
    int32_t taskbar_button_min_width{132};
    int32_t launcher_width{640};
    int32_t launcher_height{480};
    int32_t launcher_item{96};
    int32_t search_field_height{40};
    int32_t icon_sm{16};
    int32_t icon_md{20};
    int32_t icon_lg{32};
    int32_t icon_xl{48};
    int32_t row_height{32};
    int32_t focus_ring_width{2};
    int32_t min_hit_target{28};
};

enum class FontWeight : uint16_t { Regular = 400, Medium = 500, SemiBold = 600, Bold = 700 };

/// Typography metrics only. The font engine that consumes these arrives in a later stage; until then the
/// legacy 8x8 bitmap font ignores them.
struct TextStyle {
    float size_px{14.0f};
    float line_height_px{20.0f};
    FontWeight weight{FontWeight::Regular};
    float letter_spacing_px{0.0f};
};

struct TypographyTokens {
    TextStyle caption{12.0f, 16.0f, FontWeight::Regular, 0.0f};
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

[[nodiscard]] inline const Theme& default_theme() noexcept { return kVynexDarkTheme; }

/// Bridges the token theme to the legacy five-colour WindowTheme used by the current decorator, so existing
/// call sites can adopt tokens incrementally.
[[nodiscard]] constexpr WindowTheme to_window_theme(const Theme& t) noexcept {
    return WindowTheme{t.colors.surface_overlay, t.colors.surface_raised, t.colors.border, t.colors.surface,
                       t.colors.text_primary};
}

} // namespace vynexos::desktop
