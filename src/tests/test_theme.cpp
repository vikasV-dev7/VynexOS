// Design-token sanity tests: the theme is a contract, so accessibility and consistency rules are enforced here
// instead of being rediscovered by eye.
#include <cassert>
#include <iostream>
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
    assert(relative_luminance(Color{0, 0, 0, 255}) < 1e-9);
    assert(relative_luminance(Color{255, 255, 255, 255}) > 0.999);
    const double max_contrast = contrast_ratio(Color{0, 0, 0, 255}, Color{255, 255, 255, 255});
    assert(max_contrast > 20.9 && max_contrast < 21.1);
    assert(contrast_ratio(Color{10, 20, 30, 255}, Color{10, 20, 30, 255}) < 1.001);
    assert(contrast_ratio(Color{10, 20, 30, 255}, Color{200, 210, 220, 255}) == contrast_ratio(Color{200, 210, 220, 255}, Color{10, 20, 30, 255}));
    std::cout << "test_helpers passed\n";
}

void test_surface_ladder_is_ordered() {
    const auto& c = default_theme().colors;
    const double l0 = relative_luminance(c.bg_deep);
    const double l1 = relative_luminance(c.bg);
    const double l2 = relative_luminance(c.surface);
    const double l3 = relative_luminance(c.surface_raised);
    const double l4 = relative_luminance(c.surface_overlay);
    assert(l0 < l1 && l1 < l2 && l2 < l3 && l3 < l4);
    // Graphite: the lightest surface is still unmistakably dark.
    assert(l4 < 0.06);
    // Borders are visible against the surfaces they sit on, without being loud.
    assert(contrast_ratio(c.border, c.surface) > 1.3 && contrast_ratio(c.border, c.surface) < 3.0);
    assert(contrast_ratio(c.border_subtle, c.surface) > 1.05);
    assert(relative_luminance(c.border_strong) > relative_luminance(c.border));
    assert(relative_luminance(c.border) > relative_luminance(c.border_subtle));
    std::cout << "test_surface_ladder_is_ordered passed\n";
}

void test_text_contrast() {
    const auto& c = default_theme().colors;
    const Color surfaces[] = {c.bg, c.surface, c.surface_raised, c.surface_overlay};
    for (const Color& s : surfaces) {
        assert(contrast_ratio(c.text_primary, s) >= 7.0);   // WCAG AAA for primary text
        assert(contrast_ratio(c.text_secondary, s) >= 4.5); // WCAG AA for secondary text
        assert(contrast_ratio(c.accent, s) >= 3.0);         // WCAG non-text UI components
        assert(contrast_ratio(c.danger, s) >= 3.0);
        assert(contrast_ratio(c.text_disabled, s) >= 2.0);  // disabled text is exempt but must stay legible
    }
    assert(contrast_ratio(c.text_on_accent, c.accent) >= 7.0);
    assert(contrast_ratio(c.text_on_accent, c.accent_hover) >= 4.5);
    assert(contrast_ratio(c.text_on_accent, c.accent_pressed) >= 4.5);
    // The text tiers are genuinely different levels.
    assert(contrast_ratio(c.text_primary, c.surface) > contrast_ratio(c.text_secondary, c.surface));
    assert(contrast_ratio(c.text_secondary, c.surface) > contrast_ratio(c.text_disabled, c.surface));
    std::cout << "test_text_contrast passed\n";
}

void test_accent_states() {
    const auto& c = default_theme().colors;
    // Hover is lighter and pressed is darker than the resting accent.
    assert(relative_luminance(c.accent_hover) > relative_luminance(c.accent));
    assert(relative_luminance(c.accent_pressed) < relative_luminance(c.accent));
    assert(relative_luminance(c.danger_hover) > relative_luminance(c.danger));
    assert(relative_luminance(c.danger_pressed) < relative_luminance(c.danger));
    // Restraint: the accent is one cyan hue (blue and green channels dominate red).
    assert(c.accent.r < c.accent.g && c.accent.r < c.accent.b);
    assert(c.accent.b >= c.accent.g);
    // Solid tokens are opaque; tints and overlays are translucent and gentle.
    const Color solids[] = {c.bg_deep, c.bg, c.surface, c.surface_raised, c.surface_overlay, c.border_subtle, c.border, c.border_strong,
                            c.text_primary, c.text_secondary, c.text_disabled, c.text_on_accent, c.accent, c.accent_hover, c.accent_pressed,
                            c.danger, c.danger_hover, c.danger_pressed, c.success, c.warning};
    for (const Color& s : solids) assert(s.a == 255);
    assert(c.accent_subtle.a > 0 && c.accent_subtle.a <= 0x40);
    assert(c.hover_overlay.a > 0 && c.hover_overlay.a <= 0x30);
    assert(c.pressed_overlay.a > c.hover_overlay.a && c.pressed_overlay.a <= 0x40);
    assert(c.focus_ring.a > c.accent_subtle.a && c.focus_ring.a < 255);
    assert(c.scrim.a > 0 && c.scrim.a < 255);
    std::cout << "test_accent_states passed\n";
}

void test_spacing_radius_sizes() {
    const auto& t = default_theme();
    const auto& sp = t.spacing;
    assert(0 < sp.xxs && sp.xxs < sp.xs && sp.xs < sp.sm && sp.sm < sp.md && sp.md < sp.lg && sp.lg < sp.xl && sp.xl < sp.xxl);
    for (int32_t v : {sp.xs, sp.sm, sp.md, sp.lg, sp.xl, sp.xxl}) assert(v % 4 == 0); // 4px grid

    const auto& r = t.radius;
    assert(r.none == 0.0f && r.none < r.xs && r.xs < r.sm && r.sm < r.md && r.md < r.lg && r.lg < r.xl);
    assert(r.window >= r.md && r.window <= r.xl);

    const auto& z = t.size;
    // A control button fits inside the title bar with breathing room, and is a comfortable hit target.
    assert(z.titlebar_height >= z.control_button + 2 * sp.xs);
    assert(z.control_button >= z.min_hit_target);
    assert(z.taskbar_button_height + 2 * sp.xs <= z.taskbar_height);
    assert(z.search_field_height >= z.min_hit_target && z.row_height >= z.min_hit_target);
    assert(z.icon_sm < z.icon_md && z.icon_md < z.icon_lg && z.icon_lg < z.icon_xl);
    assert(z.launcher_item >= z.icon_xl + sp.lg);
    assert(z.resize_grip >= z.window_border && z.window_border >= 1);
    assert(z.focus_ring_width >= 2);
    std::cout << "test_spacing_radius_sizes passed\n";
}

void test_typography_and_elevation() {
    const auto& ty = default_theme().type;
    const TextStyle* styles[] = {&ty.caption, &ty.body, &ty.body_strong, &ty.title, &ty.heading, &ty.display};
    for (const TextStyle* s : styles) {
        assert(s->size_px >= 12.0f);                          // nothing smaller than 12px is legible on this renderer
        assert(s->line_height_px >= s->size_px * 1.2f);       // readable leading
        assert(s->line_height_px <= s->size_px * 1.6f);
    }
    assert(ty.caption.size_px < ty.body.size_px && ty.body.size_px < ty.title.size_px && ty.title.size_px < ty.heading.size_px &&
           ty.heading.size_px < ty.display.size_px);
    assert(static_cast<int>(ty.body_strong.weight) > static_cast<int>(ty.body.weight));
    assert(static_cast<int>(ty.title.weight) >= static_cast<int>(ty.body_strong.weight));

    const auto& e = default_theme().elevation;
    assert(e.control.blur < e.window_inactive.blur && e.window_inactive.blur < e.window_active.blur && e.window_active.blur < e.popup.blur);
    assert(e.control.offset_y < e.window_active.offset_y);
    for (const ShadowSpec* s : {&e.control, &e.window_inactive, &e.window_active, &e.popup}) {
        assert(s->color.a > 0 && s->color.a <= 0x90);  // restrained: shadows separate, they never dominate
        assert(s->color.r == 0 && s->color.g == 0 && s->color.b == 0);
    }
    assert(e.window_active.color.a > e.window_inactive.color.a); // focus is expressed through depth too
    std::cout << "test_typography_and_elevation passed\n";
}

void test_window_theme_bridge() {
    const Theme& t = default_theme();
    const WindowTheme w = to_window_theme(t);
    assert(same(w.active_title_color, t.colors.surface_overlay));
    assert(same(w.inactive_title_color, t.colors.surface_raised));
    assert(same(w.border_color, t.colors.border));
    assert(same(w.background_color, t.colors.surface));
    assert(same(w.text_color, t.colors.text_primary));
    // The focused title bar is distinguishable from the unfocused one.
    assert(relative_luminance(w.active_title_color) > relative_luminance(w.inactive_title_color));
    // And the default theme is a single shared instance.
    assert(&default_theme() == &kVynexDarkTheme);

    // Themes are plain values: a copy can be tweaked without touching the shared default.
    Theme custom = default_theme();
    custom.colors.accent = rgb(0xFF8800);
    assert(!same(custom.colors.accent, default_theme().colors.accent));
    std::cout << "test_window_theme_bridge passed\n";
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
    std::cout << "All theme tests passed successfully.\n";
    return 0;
}
