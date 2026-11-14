// Design-token sanity tests: the theme is a contract, so accessibility and consistency rules are enforced here
// instead of being rediscovered by eye.
#include "test_check.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include "vynexos/desktop/theme.hpp"

using namespace vynexos::desktop;

namespace {

bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

// The helpers and the default theme are usable in constant expressions.
static_assert(rgb(0x22C7DC).r == 0x22 && rgb(0x22C7DC).g == 0xC7 && rgb(0x22C7DC).b == 0xDC && rgb(0x22C7DC).a == 255);
static_assert(rgb(0x102030, 0x40).a == 0x40);
static_assert(with_alpha(Color{1, 2, 3, 255}, 9).a == 9 && with_alpha(Color{1, 2, 3, 255}, 9).b == 3);
static_assert(mix(Color{0, 0, 0, 255}, Color{200, 100, 50, 255}, 0.0f).r == 0);
static_assert(mix(Color{0, 0, 0, 255}, Color{200, 100, 50, 255}, 1.0f).r == 200);
static_assert(mix(Color{0, 0, 0, 255}, Color{200, 100, 50, 255}, 0.5f).g == 50);
static_assert(mix(Color{0, 0, 0, 255}, Color{200, 100, 50, 255}, -3.0f).r == 0);   // clamped
static_assert(mix(Color{0, 0, 0, 255}, Color{200, 100, 50, 255}, 9.0f).r == 200);  // clamped
static_assert(kVynexDarkTheme.size.titlebar_height == 36);

void test_helpers() {
    CHECK(relative_luminance(Color{0, 0, 0, 255}) < 1e-9);
    CHECK(relative_luminance(Color{255, 255, 255, 255}) > 0.999);
    const double max_contrast = contrast_ratio(Color{0, 0, 0, 255}, Color{255, 255, 255, 255});
    CHECK(max_contrast > 20.9 && max_contrast < 21.1);
    CHECK(contrast_ratio(Color{10, 20, 30, 255}, Color{10, 20, 30, 255}) < 1.001);
    CHECK(contrast_ratio(Color{10, 20, 30, 255}, Color{200, 210, 220, 255}) == contrast_ratio(Color{200, 210, 220, 255}, Color{10, 20, 30, 255}));
    std::cout << "test_helpers passed\n";
}

void test_surface_ladder_is_ordered() {
    const auto& c = default_theme().colors;
    const double l0 = relative_luminance(c.bg_deep);
    const double l1 = relative_luminance(c.bg);
    const double l2 = relative_luminance(c.surface);
    const double l3 = relative_luminance(c.surface_raised);
    const double l4 = relative_luminance(c.surface_overlay);
    CHECK(l0 < l1 && l1 < l2 && l2 < l3 && l3 < l4);
    // Graphite: the lightest surface is still unmistakably dark.
    CHECK(l4 < 0.06);
    // Borders are visible against the surfaces they sit on, without being loud.
    CHECK(contrast_ratio(c.border, c.surface) > 1.3 && contrast_ratio(c.border, c.surface) < 3.0);
    CHECK(contrast_ratio(c.border_subtle, c.surface) > 1.05);
    CHECK(relative_luminance(c.border_strong) > relative_luminance(c.border));
    CHECK(relative_luminance(c.border) > relative_luminance(c.border_subtle));
    std::cout << "test_surface_ladder_is_ordered passed\n";
}

void test_text_contrast() {
    const auto& c = default_theme().colors;
    const Color surfaces[] = {c.bg, c.surface, c.surface_raised, c.surface_overlay};
    for (const Color& s : surfaces) {
        CHECK(contrast_ratio(c.text_primary, s) >= 7.0);   // WCAG AAA for primary text
        CHECK(contrast_ratio(c.text_secondary, s) >= 4.5); // WCAG AA for secondary text
        CHECK(contrast_ratio(c.accent, s) >= 3.0);         // WCAG non-text UI components
        CHECK(contrast_ratio(c.danger, s) >= 3.0);
        CHECK(contrast_ratio(c.text_disabled, s) >= 2.0);  // disabled text is exempt but must stay legible
    }
    CHECK(contrast_ratio(c.text_on_accent, c.accent) >= 7.0);
    CHECK(contrast_ratio(c.text_on_accent, c.accent_hover) >= 4.5);
    CHECK(contrast_ratio(c.text_on_accent, c.accent_pressed) >= 4.5);
    // The text tiers are genuinely different levels.
    CHECK(contrast_ratio(c.text_primary, c.surface) > contrast_ratio(c.text_secondary, c.surface));
    CHECK(contrast_ratio(c.text_secondary, c.surface) > contrast_ratio(c.text_disabled, c.surface));
    std::cout << "test_text_contrast passed\n";
}

void test_accent_states() {
    const auto& c = default_theme().colors;
    // Hover is lighter and pressed is darker than the resting accent.
    CHECK(relative_luminance(c.accent_hover) > relative_luminance(c.accent));
    CHECK(relative_luminance(c.accent_pressed) < relative_luminance(c.accent));
    CHECK(relative_luminance(c.danger_hover) > relative_luminance(c.danger));
    CHECK(relative_luminance(c.danger_pressed) < relative_luminance(c.danger));
    // Restraint: the accent is one cyan hue (blue and green channels dominate red).
    CHECK(c.accent.r < c.accent.g && c.accent.r < c.accent.b);
    CHECK(c.accent.b >= c.accent.g);
    // Solid tokens are opaque; tints and overlays are translucent and gentle.
    const Color solids[] = {c.bg_deep, c.bg, c.surface, c.surface_raised, c.surface_overlay, c.border_subtle, c.border, c.border_strong,
                            c.text_primary, c.text_secondary, c.text_disabled, c.text_on_accent, c.accent, c.accent_hover, c.accent_pressed,
                            c.danger, c.danger_hover, c.danger_pressed, c.success, c.warning};
    for (const Color& s : solids) CHECK(s.a == 255);
    CHECK(c.accent_subtle.a > 0 && c.accent_subtle.a <= 0x40);
    CHECK(c.hover_overlay.a > 0 && c.hover_overlay.a <= 0x30);
    CHECK(c.pressed_overlay.a > c.hover_overlay.a && c.pressed_overlay.a <= 0x40);
    CHECK(c.focus_ring.a > c.accent_subtle.a && c.focus_ring.a < 255);
    CHECK(c.scrim.a > 0 && c.scrim.a < 255);
    std::cout << "test_accent_states passed\n";
}

void test_spacing_radius_sizes() {
    const auto& t = default_theme();
    const auto& sp = t.spacing;
    CHECK(0 < sp.xxs && sp.xxs < sp.xs && sp.xs < sp.sm && sp.sm < sp.md && sp.md < sp.lg && sp.lg < sp.xl && sp.xl < sp.xxl);
    for (int32_t v : {sp.xs, sp.sm, sp.md, sp.lg, sp.xl, sp.xxl}) CHECK(v % 4 == 0); // 4px grid

    const auto& r = t.radius;
    CHECK(r.none == 0.0f && r.none < r.xs && r.xs < r.sm && r.sm < r.md && r.md < r.lg && r.lg < r.xl);
    CHECK(r.window >= r.md && r.window <= r.xl);

    const auto& z = t.size;
    // A control button fits inside the title bar with breathing room, and is a comfortable hit target.
    CHECK(z.titlebar_height >= z.control_button + 2 * sp.xs);
    CHECK(z.control_button >= z.min_hit_target);
    CHECK(z.taskbar_button_height + 2 * sp.xs <= z.taskbar_height);
    CHECK(z.search_field_height >= z.min_hit_target && z.row_height >= z.min_hit_target);
    CHECK(z.icon_sm < z.icon_md && z.icon_md < z.icon_lg && z.icon_lg < z.icon_xl);
    CHECK(z.launcher_item >= z.icon_xl + sp.lg);
    CHECK(z.resize_grip >= z.window_border && z.window_border >= 1);
    CHECK(z.focus_ring_width >= 2);
    std::cout << "test_spacing_radius_sizes passed\n";
}

void test_typography_and_elevation() {
    const auto& ty = default_theme().type;
    const TextStyle* styles[] = {&ty.caption, &ty.body, &ty.body_strong, &ty.title, &ty.heading, &ty.display};
    for (const TextStyle* s : styles) {
        CHECK(s->size_px >= 12.0f);                          // nothing smaller than 12px is legible on this renderer
        CHECK(s->line_height_px >= s->size_px * 1.2f);       // readable leading
        CHECK(s->line_height_px <= s->size_px * 1.6f);
    }
    CHECK(ty.caption.size_px < ty.body.size_px && ty.body.size_px < ty.title.size_px && ty.title.size_px < ty.heading.size_px &&
           ty.heading.size_px < ty.display.size_px);
    CHECK(static_cast<int>(ty.body_strong.weight) > static_cast<int>(ty.body.weight));
    CHECK(static_cast<int>(ty.title.weight) >= static_cast<int>(ty.body_strong.weight));

    const auto& e = default_theme().elevation;
    CHECK(e.control.blur < e.window_inactive.blur && e.window_inactive.blur < e.window_active.blur && e.window_active.blur < e.popup.blur);
    CHECK(e.control.offset_y < e.window_active.offset_y);
    for (const ShadowSpec* s : {&e.control, &e.window_inactive, &e.window_active, &e.popup}) {
        CHECK(s->color.a > 0 && s->color.a <= 0x90);  // restrained: shadows separate, they never dominate
        CHECK(s->color.r == 0 && s->color.g == 0 && s->color.b == 0);
    }
    CHECK(e.window_active.color.a > e.window_inactive.color.a); // focus is expressed through depth too
    std::cout << "test_typography_and_elevation passed\n";
}

void test_window_theme_bridge() {
    const Theme& t = default_theme();
    const WindowTheme w = to_window_theme(t);
    CHECK(same(w.active_title_color, t.colors.surface_overlay));
    CHECK(same(w.inactive_title_color, t.colors.surface_raised));
    CHECK(same(w.border_color, t.colors.border));
    CHECK(same(w.background_color, t.colors.surface));
    CHECK(same(w.text_color, t.colors.text_primary));
    // The focused title bar is distinguishable from the unfocused one.
    CHECK(relative_luminance(w.active_title_color) > relative_luminance(w.inactive_title_color));
    // And the default theme is a single shared instance.
    CHECK(&default_theme() == &kVynexDarkTheme);

    // Themes are plain values: a copy can be tweaked without touching the shared default.
    Theme custom = default_theme();
    custom.colors.accent = rgb(0xFF8800);
    CHECK(!same(custom.colors.accent, default_theme().colors.accent));
    std::cout << "test_window_theme_bridge passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Stage 7.1: the approved "Signal" palettes (dark completed, light defined), token by token
// ---------------------------------------------------------------------------------------------------------

// The light theme is a constant expression too, and selecting a theme is a pure function.
static_assert(kVynexLightTheme.colors.surface.r == 0xF6 && kVynexLightTheme.colors.accent.g == 0x74);
static_assert(&theme_for(ThemeId::Dark) == &kVynexDarkTheme && &theme_for(ThemeId::Light) == &kVynexLightTheme);
static_assert(blend_over(Color{9, 8, 7, 255}, Color{1, 2, 3, 255}).r == 9);  // an opaque top replaces
static_assert(blend_over(Color{9, 8, 7, 0}, Color{1, 2, 3, 255}).r == 1);    // a transparent top changes nothing
static_assert(blend_over(Color{9, 8, 7, 0}, Color{1, 2, 3, 0}).a == 0);

struct Approved {
    const char* name;
    uint32_t rgb;
    uint8_t alpha;
};

// The Stage 7.0 specification, section 4. Order = declaration order in ColorTokens. One value is a deliberate
// correction of the table: dark danger_solid is #CA383E, not the table's #D03A40 (see test_danger_tokens below and the
// comment in theme.hpp). The light theme's danger_hover and danger_pressed are derived, not tabled (also below).
constexpr Approved kApprovedDark[] = {
    {"bg_deep", 0x0E1216, 255},        {"bg", 0x12171C, 255},              {"surface_sunken", 0x151A20, 255}, {"surface", 0x1A2027, 255},
    {"surface_raised", 0x222A33, 255}, {"surface_overlay", 0x2A333D, 255}, {"border_subtle", 0x28313A, 255},  {"border", 0x36414D, 255},
    {"border_strong", 0x4A5663, 255},  {"control_border", 0x66727F, 255},  {"text_primary", 0xE8EEF4, 255},   {"text_secondary", 0x9AA7B4, 255},
    {"text_disabled", 0x5C6773, 255},  {"text_on_accent", 0x04222A, 255},  {"text_on_danger", 0xFFFFFF, 255}, {"accent", 0x22C7DC, 255},
    {"accent_hover", 0x4BD8EA, 255},   {"accent_pressed", 0x159FB2, 255},  {"accent_subtle", 0x22C7DC, 0x26}, {"focus_ring", 0x22C7DC, 0xB0},
    {"selection_text", 0x22C7DC, 0x59}, {"danger", 0xE5484D, 255},         {"danger_hover", 0xF2666B, 255},   {"danger_pressed", 0xC13338, 255},
    {"danger_solid", 0xCA383E, 255},   {"danger_text", 0xF47A7E, 255},     {"success", 0x3DD68C, 255},        {"warning", 0xF5A524, 255},
    {"info", 0x5AA9F5, 255},           {"hover_overlay", 0xFFFFFF, 0x12},  {"pressed_overlay", 0xFFFFFF, 0x1E}, {"scrim", 0x000000, 0x80},
};
constexpr Approved kApprovedLight[] = {
    {"bg_deep", 0xC9D1D9, 255},        {"bg", 0xDCE2E8, 255},              {"surface_sunken", 0xEEF1F4, 255}, {"surface", 0xF6F8FA, 255},
    {"surface_raised", 0xFCFDFE, 255}, {"surface_overlay", 0xFFFFFF, 255}, {"border_subtle", 0xE3E8ED, 255},  {"border", 0xCDD5DD, 255},
    {"border_strong", 0xA9B4BF, 255},  {"control_border", 0x7D8996, 255},  {"text_primary", 0x111820, 255},   {"text_secondary", 0x4A5764, 255},
    {"text_disabled", 0x98A3AE, 255},  {"text_on_accent", 0xFFFFFF, 255},  {"text_on_danger", 0xFFFFFF, 255}, {"accent", 0x087482, 255},
    {"accent_hover", 0x076976, 255},   {"accent_pressed", 0x055963, 255},  {"accent_subtle", 0x087482, 0x1F}, {"focus_ring", 0x087482, 0xCC},
    {"selection_text", 0x087482, 0x40}, {"danger", 0xC8323A, 255},         {"danger_hover", 0xB42D34, 255},   {"danger_pressed", 0xA0282E, 255},
    {"danger_solid", 0xC8323A, 255},   {"danger_text", 0xB4262E, 255},     {"success", 0x137547, 255},        {"warning", 0x955800, 255},
    {"info", 0x1F62C4, 255},           {"hover_overlay", 0x000000, 0x0D},  {"pressed_overlay", 0x000000, 0x17}, {"scrim", 0x000000, 0x66},
};
static_assert(std::size(kApprovedDark) == kColorTokenCount && std::size(kApprovedLight) == kColorTokenCount);

template <size_t N>
void expect_palette(const ColorTokens& colors, const Approved (&approved)[N], const char* theme_name) {
    size_t index = 0;
    for_each_color_token(colors, [&](const char* name, Color value) {
        vynex_test::context() = std::string(theme_name) + "." + name;
        CHECK(index < N);
        CHECK(std::string(name) == approved[index].name); // every token, in order, none missing or extra
        CHECK(same(value, rgb(approved[index].rgb, approved[index].alpha)));
        ++index;
    });
    CHECK(index == N);
    vynex_test::context().clear();
}

void test_exact_approved_token_values() {
    expect_palette(kVynexDarkTheme.colors, kApprovedDark, "dark");
    expect_palette(kVynexLightTheme.colors, kApprovedLight, "light");

    // Token names are unique, and every token is reachable by name.
    std::vector<std::string> names;
    for_each_color_token(kVynexDarkTheme.colors, [&](const char* name, Color) { names.emplace_back(name); });
    CHECK(names.size() == kColorTokenCount);
    std::sort(names.begin(), names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());

    // The light palette is its own set of values: only tokens that are the same colour by design coincide with dark.
    std::vector<std::string> shared;
    size_t i = 0;
    for_each_color_token(kVynexLightTheme.colors, [&](const char* name, Color value) {
        if (same(value, rgb(kApprovedDark[i].rgb, kApprovedDark[i].alpha))) shared.emplace_back(name);
        ++i;
    });
    CHECK(shared == std::vector<std::string>{"text_on_danger"}); // white on a danger fill in both themes
    std::cout << "test_exact_approved_token_values passed\n";
}

void test_theme_selection_is_deterministic() {
    CHECK(&default_theme() == &kVynexDarkTheme);             // dark is the default
    CHECK(&theme_for(ThemeId::Dark) == &default_theme());
    CHECK(&theme_for(ThemeId::Light) == &kVynexLightTheme);
    CHECK(&theme_for(ThemeId::Light) == &theme_for(ThemeId::Light)); // one instance per id, every time
    CHECK(&theme_for(ThemeId::Dark) != &theme_for(ThemeId::Light));

    // The two themes differ in colour only: geometry, type and elevation are shared, so a layout is the same in both.
    const Theme& d = kVynexDarkTheme;
    const Theme& l = kVynexLightTheme;
    CHECK(d.spacing.xxs == l.spacing.xxs && d.spacing.md == l.spacing.md && d.spacing.xxxl == l.spacing.xxxl);
    CHECK(d.radius.control == l.radius.control && d.radius.window == l.radius.window);
    CHECK(d.size.taskbar_height == l.size.taskbar_height && d.size.control_height == l.size.control_height &&
          d.size.launcher_width == l.size.launcher_width && d.size.launcher_height == l.size.launcher_height);
    CHECK(d.type.body.size_px == l.type.body.size_px && d.type.metadata.weight == l.type.metadata.weight);
    CHECK(d.elevation.popup.blur == l.elevation.popup.blur);
    std::cout << "test_theme_selection_is_deterministic passed\n";
}

void test_blend_over() {
    const ColorTokens& d = kVynexDarkTheme.colors;
    const ColorTokens& l = kVynexLightTheme.colors;
    // The state fills components are drawn with (the values the Stage 7.0 contrast figures were computed for).
    CHECK(same(blend_over(d.hover_overlay, d.surface), rgb(0x2A3036)));
    CHECK(same(blend_over(d.accent_subtle, d.surface), rgb(0x1B3942)));
    CHECK(same(blend_over(l.hover_overlay, l.surface), rgb(0xE9EBED)));
    CHECK(same(blend_over(l.accent_subtle, l.surface), rgb(0xD9E8EB)));
    // Over a transparent surface a translucent colour is itself; alpha accumulates over a translucent one.
    CHECK(same(blend_over(d.hover_overlay, Color{0, 0, 0, 0}), d.hover_overlay));
    const Color stacked = blend_over(Color{255, 255, 255, 128}, Color{0, 0, 0, 128});
    CHECK(stacked.a > 128 && stacked.a < 255 && stacked.r > 128);
    std::cout << "test_blend_over passed\n";
}

void test_semantic_relationships_in_both_themes() {
    for (const ThemeId id : {ThemeId::Dark, ThemeId::Light}) {
        const bool dark = id == ThemeId::Dark;
        vynex_test::context() = dark ? "dark" : "light";
        const ColorTokens& c = theme_for(id).colors;

        // Elevation always climbs towards the light: each surface is lighter than the one below it.
        const Color ladder[] = {c.bg_deep, c.bg, c.surface_sunken, c.surface, c.surface_raised, c.surface_overlay};
        for (size_t i = 1; i < std::size(ladder); ++i) CHECK(relative_luminance(ladder[i]) > relative_luminance(ladder[i - 1]));
        // Dark stays graphite, light stays light.
        CHECK(dark ? relative_luminance(c.surface_overlay) < 0.06 : relative_luminance(c.bg_deep) > 0.55);

        // Borders are a scale of emphasis: each stands out from the surface more than the last.
        const Color borders[] = {c.border_subtle, c.border, c.border_strong, c.control_border};
        for (size_t i = 1; i < std::size(borders); ++i) CHECK(contrast_ratio(borders[i], c.surface) > contrast_ratio(borders[i - 1], c.surface));
        CHECK(contrast_ratio(c.border_subtle, c.surface) > 1.05);
        CHECK(contrast_ratio(c.border, c.surface) > 1.3 && contrast_ratio(c.border, c.surface) < 3.0); // visible, not loud

        // Text tiers are different levels, in the same order everywhere.
        CHECK(contrast_ratio(c.text_primary, c.surface) > contrast_ratio(c.text_secondary, c.surface));
        CHECK(contrast_ratio(c.text_secondary, c.surface) > contrast_ratio(c.text_disabled, c.surface));

        // Interaction: pressed is always darker than hover. On dark surfaces hover lightens the accent; on light
        // surfaces it darkens it (more ink, not less).
        CHECK(relative_luminance(c.accent_pressed) < relative_luminance(c.accent_hover));
        CHECK(relative_luminance(c.danger_pressed) < relative_luminance(c.danger_hover));
        if (dark) {
            CHECK(relative_luminance(c.accent_hover) > relative_luminance(c.accent));
            CHECK(relative_luminance(c.danger_hover) > relative_luminance(c.danger));
            CHECK(c.hover_overlay.r == 255 && c.pressed_overlay.r == 255); // overlays lighten
        } else {
            CHECK(relative_luminance(c.accent_hover) < relative_luminance(c.accent));
            CHECK(relative_luminance(c.danger_hover) < relative_luminance(c.danger));
            CHECK(c.hover_overlay.r == 0 && c.pressed_overlay.r == 0);     // overlays darken
        }
        CHECK(c.pressed_overlay.a > c.hover_overlay.a);

        // One signal: a single cyan hue, and every accent-derived token is that accent at some opacity.
        CHECK(c.accent.r < c.accent.g && c.accent.r < c.accent.b && c.accent.b >= c.accent.g);
        for (const Color derived : {c.accent_subtle, c.focus_ring, c.selection_text}) {
            CHECK(derived.r == c.accent.r && derived.g == c.accent.g && derived.b == c.accent.b);
            CHECK(derived.a > 0 && derived.a < 255);
        }
        CHECK(c.accent_subtle.a < c.selection_text.a && c.selection_text.a < c.focus_ring.a);

        // The four status colours are four different hues.
        const Color status[] = {c.danger, c.success, c.warning, c.info};
        CHECK(c.danger.r > c.danger.g && c.danger.r > c.danger.b);     // red
        CHECK(c.success.g > c.success.r && c.success.g > c.success.b); // green
        CHECK(c.warning.r > c.warning.b && c.warning.g > c.warning.b); // amber
        CHECK(c.info.b > c.info.r && c.info.b > c.info.g);             // blue
        for (const Color& s : status) CHECK(s.a == 255);

        // Solid tokens are opaque; tints and overlays are translucent.
        size_t translucent = 0;
        for_each_color_token(c, [&](const char*, Color value) { translucent += (value.a != 255); });
        CHECK(translucent == 6); // accent_subtle, focus_ring, selection_text, hover_overlay, pressed_overlay, scrim
        CHECK(c.scrim.r == 0 && c.scrim.g == 0 && c.scrim.b == 0 && c.scrim.a >= 0x60 && c.scrim.a <= 0x90);
    }
    vynex_test::context().clear();
    std::cout << "test_semantic_relationships_in_both_themes passed\n";
}

void test_wcag_contrast_in_both_themes() {
    for (const ThemeId id : {ThemeId::Dark, ThemeId::Light}) {
        vynex_test::context() = id == ThemeId::Dark ? "dark" : "light";
        const ColorTokens& c = theme_for(id).colors;
        // Everything text can sit on, including the fills the state model produces.
        const Color selected = blend_over(c.accent_subtle, c.surface);          // a selected row
        const Color hovered = blend_over(c.hover_overlay, c.surface);           // a hovered row
        const Color pressed = blend_over(c.pressed_overlay, c.surface);         // a pressed row
        const Color pressed_button = blend_over(c.pressed_overlay, c.surface_raised); // a pressed default button
        const Color surfaces[] = {c.surface_sunken, c.surface, c.surface_raised, c.surface_overlay};
        for (const Color& s : surfaces) {
            CHECK(contrast_ratio(c.text_primary, s) >= 7.0);    // AAA, body text
            CHECK(contrast_ratio(c.text_secondary, s) >= 4.5);  // AA
            CHECK(contrast_ratio(c.text_disabled, s) >= 2.0);   // exempt, but still legible
            CHECK(contrast_ratio(c.accent, s) >= 4.5);          // accent text (links) and the keyline
            CHECK(contrast_ratio(c.danger_text, s) >= 4.5);
            CHECK(contrast_ratio(c.success, s) >= 4.5);
            CHECK(contrast_ratio(c.warning, s) >= 4.5);
            CHECK(contrast_ratio(c.info, s) >= 4.5);
            CHECK(contrast_ratio(c.danger, s) >= 3.0);          // icons and indicators (non-text)
        }
        for (const Color& s : {selected, hovered, pressed}) {
            CHECK(contrast_ratio(c.text_primary, s) >= 7.0);
            CHECK(contrast_ratio(c.text_secondary, s) >= 4.5); // rows carry secondary text (sizes, dates) too
        }
        CHECK(contrast_ratio(c.text_primary, pressed_button) >= 7.0); // a button's label is primary text
        // Text on filled buttons, in every state of the fill.
        for (const Color& fill : {c.accent, c.accent_hover, c.accent_pressed}) CHECK(contrast_ratio(c.text_on_accent, fill) >= 4.5);
        CHECK(contrast_ratio(c.text_on_danger, c.danger_solid) >= 4.5);
        CHECK(contrast_ratio(c.text_on_danger, c.danger_pressed) >= 4.5);
        CHECK(contrast_ratio(c.text_on_danger, blend_over(c.hover_overlay, c.danger_solid)) >= 4.5); // a hovered destructive button
        // Boundaries of interactive controls and the focus ring: 3:1 against what they sit on (WCAG 1.4.11).
        CHECK(contrast_ratio(c.control_border, c.surface_sunken) >= 3.0);
        CHECK(contrast_ratio(c.control_border, c.surface) >= 3.0);
        const Color ring = with_alpha(c.focus_ring, 255);
        for (const Color& s : {c.bg, c.surface_sunken, c.surface, c.surface_raised, c.surface_overlay}) CHECK(contrast_ratio(ring, s) >= 3.0);
        // The composited ring (it is translucent) still clears 3:1 on the window body.
        CHECK(contrast_ratio(blend_over(c.focus_ring, c.surface), c.surface) >= 3.0);
    }
    vynex_test::context().clear();
    std::cout << "test_wcag_contrast_in_both_themes passed\n";
}

// Channel-wise scale with rounding, the derivation used for the corrected and the derived danger values.
Color scaled(Color c, double factor) {
    auto ch = [factor](uint8_t v) { return static_cast<uint8_t>(static_cast<double>(v) * factor + 0.5); };
    return Color{ch(c.r), ch(c.g), ch(c.b), c.a};
}

void test_danger_tokens_and_derivation() {
    // --- Dark: danger_solid is the Stage 7.0 table's #D03A40 scaled by 0.97, which fixes the hovered label contrast. ---
    const ColorTokens& d = kVynexDarkTheme.colors;
    const Color table_value = rgb(0xD03A40);
    CHECK(same(d.danger_solid, scaled(table_value, 0.97)) && same(d.danger_solid, rgb(0xCA383E)));
    // The reason: with the table's value the hovered fill is #D3484D and the white label 4.37:1 (below AA).
    const Color table_hover = blend_over(d.hover_overlay, table_value);
    CHECK(same(table_hover, rgb(0xD3484D)));
    CHECK(contrast_ratio(d.text_on_danger, table_hover) < 4.5 && contrast_ratio(d.text_on_danger, table_hover) > 4.36);
    // With the corrected value, every state of the label clears AA with margin.
    const Color hover_fill = blend_over(d.hover_overlay, d.danger_solid);
    CHECK(same(hover_fill, rgb(0xCE464C)));
    const double rest = contrast_ratio(d.text_on_danger, d.danger_solid);
    const double hover = contrast_ratio(d.text_on_danger, hover_fill);
    const double pressed = contrast_ratio(d.text_on_danger, d.danger_pressed); // pressed uses the danger_pressed token
    CHECK(rest > 5.0 && rest < 5.1);
    CHECK(hover >= 4.5 && hover < 4.65);
    CHECK(pressed > 5.5 && pressed < 5.6);
    // The states keep their order: hover lightens, pressed darkens (relative to the resting fill).
    CHECK(relative_luminance(hover_fill) > relative_luminance(d.danger_solid));
    CHECK(relative_luminance(d.danger_pressed) < relative_luminance(d.danger_solid));
    // The correction is a few units of one hue: it did not move the red out of the status family or change any other token.
    CHECK(d.danger_solid.r > d.danger_solid.g && d.danger_solid.r > d.danger_solid.b);
    CHECK(std::abs(int{d.danger_solid.r} - int{table_value.r}) <= 8 && std::abs(int{d.danger_solid.g} - int{table_value.g}) <= 8 &&
          std::abs(int{d.danger_solid.b} - int{table_value.b}) <= 8);

    // --- Light: danger_hover / danger_pressed are DERIVED from danger (x0.9 and x0.8), not tabled values. ---
    const ColorTokens& l = kVynexLightTheme.colors;
    CHECK(same(l.danger_hover, scaled(l.danger, 0.9)) && same(l.danger_hover, rgb(0xB42D34)));
    CHECK(same(l.danger_pressed, scaled(l.danger, 0.8)) && same(l.danger_pressed, rgb(0xA0282E)));
    CHECK(relative_luminance(l.danger_pressed) < relative_luminance(l.danger_hover) &&
          relative_luminance(l.danger_hover) < relative_luminance(l.danger));
    // Contrast of the derived values: white text on them, and as icons or indicators on every light surface.
    CHECK(contrast_ratio(l.text_on_danger, l.danger_hover) > 6.2 && contrast_ratio(l.text_on_danger, l.danger_pressed) > 7.3);
    for (const Color& surface : {l.bg, l.surface_sunken, l.surface, l.surface_raised, l.surface_overlay}) {
        CHECK(contrast_ratio(l.danger_hover, surface) >= 4.5);   // above the 3:1 non-text requirement, and AA for text
        CHECK(contrast_ratio(l.danger_pressed, surface) >= 4.5);
    }
    // The light destructive button: the hover overlay darkens the fill, so its label only gains contrast.
    const Color light_hover = blend_over(l.hover_overlay, l.danger_solid);
    CHECK(same(light_hover, rgb(0xBE2F37)));
    CHECK(contrast_ratio(l.text_on_danger, l.danger_solid) > 5.2);
    CHECK(contrast_ratio(l.text_on_danger, light_hover) > contrast_ratio(l.text_on_danger, l.danger_solid));
    std::cout << "test_danger_tokens_and_derivation passed\n";
}

void test_stage_7_geometry_tokens() {
    const Theme& t = default_theme();
    const auto& sp = t.spacing;
    CHECK(sp.xxs == 2 && sp.xs == 4 && sp.sm == 8 && sp.md == 12 && sp.lg == 16 && sp.xl == 24 && sp.xxl == 32 && sp.xxxl == 48);
    CHECK(sp.xxl < sp.xxxl && sp.xxxl % 4 == 0);

    // Four radii carry the shape language: 4, 6, 8 and 10, strictly increasing with the size of what they round.
    const auto& r = t.radius;
    CHECK(r.sm == 4.0f && r.control == 6.0f && r.md == 8.0f && r.window == 10.0f);
    CHECK(r.sm < r.control && r.control < r.md && r.md < r.window);
    for (const float v : {r.none, r.xs, r.sm, r.control, r.md, r.lg, r.xl, r.window}) CHECK(v >= 0.0f && v <= 16.0f);

    const auto& z = t.size;
    CHECK(z.border_width == 1 && z.window_border == z.border_width);
    CHECK(z.titlebar_height == 36);                                  // standard window header
    CHECK(z.taskbar_height == 48);
    CHECK(z.launcher_width == 420 && z.launcher_height == 520);
    CHECK(z.control_height_sm == 24 && z.control_height == 32 && z.control_height_lg == 40);
    CHECK(z.search_field_height == z.control_height_lg);
    CHECK(z.row_height_compact == 28 && z.row_height == 32);
    CHECK(z.icon_button == 28 && z.icon_button >= z.min_hit_target && z.control_height >= z.min_hit_target);
    CHECK(z.icon_sm == 16 && z.icon_md == 20 && z.icon_24 == 24 && z.icon_lg == 32 && z.icon_xl == 48);
    CHECK(z.icon_md < z.icon_24 && z.icon_24 < z.icon_lg);
    CHECK(z.selection_indicator == 2 && z.focus_ring_width == 2 && z.focus_ring_offset == 2);
    // Everything sits on the 4 px grid except hairlines and the keyline.
    for (const int32_t v : {z.titlebar_height, z.taskbar_height, z.launcher_width, z.launcher_height, z.control_height_sm, z.control_height,
                            z.control_height_lg, z.row_height_compact, z.row_height, z.icon_button, z.icon_sm, z.icon_md, z.icon_24, z.icon_lg,
                            z.icon_xl, z.taskbar_button_height}) {
        CHECK(v > 0 && v % 4 == 0);
    }
    // A control and its focus ring fit in a row's worth of space; a taskbar button fits in the taskbar.
    CHECK(z.control_height + 2 * (z.focus_ring_offset + z.focus_ring_width) <= z.taskbar_height);
    CHECK(z.taskbar_button_height + 2 * sp.xs <= z.taskbar_height);
    // The launcher fits a 1280x720 desktop above the taskbar with its margins (the shell shrinks it on smaller ones).
    CHECK(z.launcher_height + z.taskbar_height + 2 * sp.md <= 720 && z.launcher_width + 2 * sp.md <= 640);

    // Typography: the metadata style joins the scale; nothing is smaller than 12 px.
    const auto& ty = t.type;
    CHECK(ty.metadata.size_px == 12.0f && ty.metadata.line_height_px == 16.0f && ty.metadata.weight == FontWeight::Medium);
    CHECK(ty.metadata.size_px == ty.caption.size_px && static_cast<int>(ty.metadata.weight) > static_cast<int>(ty.caption.weight));
    CHECK(ty.caption.size_px == 12.0f && ty.body.size_px == 14.0f && ty.body_strong.size_px == 14.0f && ty.title.size_px == 15.0f &&
          ty.heading.size_px == 20.0f && ty.display.size_px == 32.0f);
    for (const TextStyle* s : {&ty.caption, &ty.metadata, &ty.body, &ty.body_strong, &ty.title, &ty.heading, &ty.display}) {
        CHECK(s->size_px >= 12.0f && s->line_height_px >= s->size_px * 1.2f && s->line_height_px <= s->size_px * 1.6f);
        CHECK(s->letter_spacing_px == 0.0f); // the renderer does not apply letter spacing, so no style may ask for it
    }
    std::cout << "test_stage_7_geometry_tokens passed\n";
}

} // namespace

int main() {
    std::cout << "Running Theme Token Tests...\n";
    test_helpers();
    test_surface_ladder_is_ordered();
    test_text_contrast();
    test_accent_states();
    test_spacing_radius_sizes();
    test_typography_and_elevation();
    test_window_theme_bridge();
    test_exact_approved_token_values();
    test_theme_selection_is_deterministic();
    test_blend_over();
    test_semantic_relationships_in_both_themes();
    test_wcag_contrast_in_both_themes();
    test_stage_7_geometry_tokens();
    test_danger_tokens_and_derivation();
    std::cout << "All theme tests passed successfully.\n";
    return 0;
}
