// Pixel-level tests for the anti-aliased Painter and its integration with BasicWidgetToolkit.
//
// Covers: exact fills, straight-alpha blending (including onto transparent / translucent destinations),
// anti-aliased shapes (symmetry, area), strokes, lines, gradients, clipping, shadows, bounds safety under a
// randomised fuzz with guard bytes, and the guarantee that drawing performs no heap allocation.
#include <algorithm>
#include <array>
#include <atomic>
#include "test_check.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/theme.hpp"

using namespace vynexos::desktop;

// ---------------------------------------------------------------------------------------------------------
// Allocation counter: lets a test prove that a region of code performs no heap allocation.
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

// ---------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------
namespace {

struct Px {
    uint8_t r, g, b, a;
    bool operator==(const Px&) const = default;
};

constexpr Color kWhite{255, 255, 255, 255};
constexpr Color kRed{255, 0, 0, 255};

Px get(SoftwareSurface& s, int x, int y) {
    auto m = s.map_pixels();
    const size_t i = (static_cast<size_t>(y) * s.width() + static_cast<size_t>(x)) * 4;
    Px p{m[i], m[i + 1], m[i + 2], m[i + 3]};
    s.unmap_pixels();
    return p;
}

int alpha(SoftwareSurface& s, int x, int y) { return get(s, x, y).a; }

// Sum of alpha / 255 over the whole surface = coverage area in pixels.
double area(SoftwareSurface& s) {
    auto m = s.map_pixels();
    double sum = 0;
    for (size_t i = 3; i < m.size(); i += 4) sum += m[i];
    s.unmap_pixels();
    return sum / 255.0;
}

size_t count_alpha_nonzero(SoftwareSurface& s) {
    auto m = s.map_pixels();
    size_t n = 0;
    for (size_t i = 3; i < m.size(); i += 4) n += (m[i] != 0);
    s.unmap_pixels();
    return n;
}

size_t count_alpha_nonzero_outside(SoftwareSurface& s, int x0, int y0, int x1, int y1) {
    auto m = s.map_pixels();
    size_t n = 0;
    for (uint32_t y = 0; y < s.height(); ++y) {
        for (uint32_t x = 0; x < s.width(); ++x) {
            const bool inside = static_cast<int>(x) >= x0 && static_cast<int>(x) < x1 && static_cast<int>(y) >= y0 && static_cast<int>(y) < y1;
            if (!inside && m[(y * s.width() + x) * 4 + 3] != 0) ++n;
        }
    }
    s.unmap_pixels();
    return n;
}

bool near(int a, int b, int tol) { return std::abs(a - b) <= tol; }

bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool near_rel(double v, double expected, double rel) { return std::fabs(v - expected) <= expected * rel; }

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------------------------------------
// fill_rect
// ---------------------------------------------------------------------------------------------------------
void test_fill_rect_exact() {
    SoftwareSurface s(20, 20);
    {
        Painter p(s);
        p.fill_rect({5, 5, 10, 10}, kRed);
    }
    CHECK(get(s, 5, 5) == (Px{255, 0, 0, 255}));
    CHECK(get(s, 14, 14) == (Px{255, 0, 0, 255}));
    CHECK(alpha(s, 4, 5) == 0 && alpha(s, 15, 14) == 0 && alpha(s, 5, 4) == 0 && alpha(s, 14, 15) == 0);
    CHECK(count_alpha_nonzero(s) == 100);
    std::cout << "test_fill_rect_exact passed\n";
}

void test_fill_rect_bounds_and_degenerate() {
    {
        SoftwareSurface s(20, 20);
        {
            Painter p(s);
            p.fill_rect({-10, -10, 15, 15}, kRed);
            p.fill_rect({15, 15, 100, 100}, kRed);
        }
        CHECK(count_alpha_nonzero(s) == 50);
        CHECK(alpha(s, 4, 4) == 255 && alpha(s, 5, 5) == 0 && alpha(s, 15, 15) == 255 && alpha(s, 14, 14) == 0);
    }
    {
        // Extremes of Rect must not overflow: x + width spans exactly the whole int32 positive half.
        SoftwareSurface s(20, 20);
        {
            Painter p(s);
            p.fill_rect({std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(), std::numeric_limits<uint32_t>::max(),
                         std::numeric_limits<uint32_t>::max()},
                        kRed);
        }
        CHECK(count_alpha_nonzero(s) == 400);
    }
    {
        SoftwareSurface s(20, 20);
        {
            Painter p(s);
            p.fill_rect({0, 0, 0, 10}, kRed);
            p.fill_rect({0, 0, 10, 0}, kRed);
            p.fill_rect({100, 100, 5, 5}, kRed);
            p.fill_rect({-100, -100, 5, 5}, kRed);
            p.fill_rect({0, 0, 5, 5}, Color{1, 2, 3, 0});
            p.fill_rect({std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(), 10, 10}, kRed);
        }
        CHECK(count_alpha_nonzero(s) == 0);
    }
    std::cout << "test_fill_rect_bounds_and_degenerate passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Blending
// ---------------------------------------------------------------------------------------------------------
void test_blend_onto_transparent_keeps_colour() {
    SoftwareSurface s(4, 4);
    {
        Painter p(s);
        p.fill_rect({0, 0, 2, 2}, Color{255, 0, 0, 128});
    }
    // The legacy blend darkened colour by alpha when the destination was transparent. Straight-alpha compositing
    // must keep the colour intact and carry the alpha.
    CHECK(get(s, 0, 0) == (Px{255, 0, 0, 128}));
    std::cout << "test_blend_onto_transparent_keeps_colour passed\n";
}

void test_blend_onto_opaque() {
    SoftwareSurface s(4, 4);
    {
        Painter p(s);
        p.fill_rect({0, 0, 4, 4}, kWhite);
        p.fill_rect({0, 0, 2, 2}, Color{255, 0, 0, 128});
        p.fill_rect({2, 2, 2, 2}, Color{0, 0, 0, 255});
        p.fill_rect({0, 2, 2, 2}, Color{0, 0, 0, 0}); // no-op
    }
    const Px p = get(s, 0, 0);
    CHECK(p.r == 255 && near(p.g, 127, 1) && near(p.b, 127, 1) && p.a == 255);
    CHECK(get(s, 3, 3) == (Px{0, 0, 0, 255}));
    CHECK(get(s, 0, 3) == (Px{255, 255, 255, 255}));
    std::cout << "test_blend_onto_opaque passed\n";
}

void test_blend_translucent_over_translucent() {
    SoftwareSurface s(4, 4);
    {
        Painter p(s);
        p.fill_rect({0, 0, 1, 1}, Color{255, 255, 255, 128});
        p.fill_rect({0, 0, 1, 1}, Color{255, 255, 255, 128}); // two 50% layers -> ~75%
        p.fill_rect({1, 0, 1, 1}, Color{0, 0, 255, 128});
        p.fill_rect({1, 0, 1, 1}, Color{255, 0, 0, 128});
    }
    const Px a = get(s, 0, 0);
    CHECK(a.r >= 254 && a.g >= 254 && a.b >= 254); // colour must stay white, not drift
    CHECK(near(a.a, 192, 1));                      // 128 + 128*(127/255)
    const Px b = get(s, 1, 0);
    CHECK(near(b.a, 192, 1));
    CHECK(near(b.r, 170, 2) && near(b.b, 85, 2) && b.g == 0); // newer (red) layer carries 2/3 of the weight
    std::cout << "test_blend_translucent_over_translucent passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Rounded rectangles
// ---------------------------------------------------------------------------------------------------------
void test_rounded_rect_corners_and_edges() {
    SoftwareSurface s(40, 40);
    {
        Painter p(s);
        p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::uniform(8.0f), kWhite);
    }
    CHECK(alpha(s, 4, 4) == 0 && alpha(s, 5, 5) == 0); // outside the arc
    const int partial = alpha(s, 6, 6);                 // straddles the arc: anti-aliased
    CHECK(partial > 100 && partial < 255);
    CHECK(alpha(s, 7, 7) == 255);                      // inside the arc
    CHECK(alpha(s, 20, 4) == 255 && alpha(s, 4, 20) == 255 && alpha(s, 35, 20) == 255 && alpha(s, 20, 35) == 255);
    CHECK(alpha(s, 20, 3) == 0 && alpha(s, 3, 20) == 0 && alpha(s, 36, 20) == 0 && alpha(s, 20, 36) == 0);
    CHECK(alpha(s, 20, 20) == 255);
    std::cout << "test_rounded_rect_corners_and_edges passed\n";
}

void test_rounded_rect_symmetry_and_area() {
    SoftwareSurface s(40, 40);
    {
        Painter p(s);
        p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::uniform(8.0f), kWhite);
    }
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const int a = alpha(s, 4 + x, 4 + y);
            CHECK(a == alpha(s, 4 + 31 - x, 4 + y));
            CHECK(a == alpha(s, 4 + x, 4 + 31 - y));
        }
    }
    const double expected = 32.0 * 32.0 - (4.0 - kPi) * 8.0 * 8.0; // square minus four corner voids
    CHECK(near_rel(area(s), expected, 0.01));
    std::cout << "test_rounded_rect_symmetry_and_area passed\n";
}

void test_rounded_rect_radius_clamp_and_zero() {
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::uniform(1000.0f), kWhite); }
        CHECK(near_rel(area(s), kPi * 16.0 * 16.0, 0.02)); // clamped to half the side: a circle
    }
    {
        SoftwareSurface s(60, 24);
        { Painter p(s); p.fill_rounded_rect({4, 4, 40, 16}, CornerRadii::uniform(1000.0f), kWhite); }
        CHECK(near_rel(area(s), kPi * 8.0 * 8.0 + 24.0 * 16.0, 0.02)); // pill
    }
    {
        SoftwareSurface a(30, 30), b(30, 30);
        { Painter p(a); p.fill_rounded_rect({3, 3, 20, 20}, CornerRadii::uniform(0.0f), kRed); }
        { Painter p(b); p.fill_rect({3, 3, 20, 20}, kRed); }
        CHECK(area(a) == area(b));
        for (int y = 0; y < 30; ++y) for (int x = 0; x < 30; ++x) CHECK(get(a, x, y) == get(b, x, y));
    }
    {
        SoftwareSurface s(30, 30);
        {
            Painter p(s);
            p.fill_rounded_rect({3, 3, 20, 20}, CornerRadii::uniform(-5.0f), kRed);
            p.fill_rounded_rect({3, 3, 20, 20}, CornerRadii::uniform(std::nanf("")), kRed);
        }
        CHECK(count_alpha_nonzero(s) == 400); // negative / NaN radii behave as square
    }
    std::cout << "test_rounded_rect_radius_clamp_and_zero passed\n";
}

void test_rounded_rect_per_corner() {
    SoftwareSurface s(40, 40);
    {
        Painter p(s);
        p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::top(10.0f), kWhite);
    }
    CHECK(alpha(s, 4, 4) == 0 && alpha(s, 35, 4) == 0); // rounded top corners
    CHECK(alpha(s, 4, 35) == 255 && alpha(s, 35, 35) == 255); // square bottom corners
    std::cout << "test_rounded_rect_per_corner passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Strokes
// ---------------------------------------------------------------------------------------------------------
void test_stroke_square() {
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.stroke_rounded_rect({2, 2, 36, 36}, CornerRadii{}, 1.0f, kWhite); }
        CHECK(alpha(s, 2, 2) == 255 && alpha(s, 20, 2) == 255 && alpha(s, 2, 20) == 255 && alpha(s, 37, 37) == 255);
        CHECK(alpha(s, 3, 3) == 0 && alpha(s, 20, 20) == 0 && alpha(s, 20, 3) == 0);
        CHECK(alpha(s, 20, 1) == 0 && alpha(s, 1, 20) == 0);
        size_t full = 0;
        auto m = s.map_pixels();
        for (size_t i = 3; i < m.size(); i += 4) full += (m[i] == 255);
        s.unmap_pixels();
        CHECK(full == 4 * 36 - 4);
    }
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.stroke_rounded_rect({2, 2, 36, 36}, CornerRadii{}, 2.0f, kWhite); }
        CHECK(alpha(s, 3, 20) == 255 && alpha(s, 4, 20) == 0);
    }
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.stroke_rounded_rect({2, 2, 36, 36}, CornerRadii{}, 500.0f, kWhite); }
        CHECK(count_alpha_nonzero(s) == 36 * 36); // thicker than the shape: solid
        CHECK(alpha(s, 20, 20) == 255);
    }
    std::cout << "test_stroke_square passed\n";
}

void test_stroke_rounded() {
    SoftwareSurface s(60, 60);
    { Painter p(s); p.stroke_rounded_rect({4, 4, 52, 52}, CornerRadii::uniform(8.0f), 1.5f, kWhite); }
    CHECK(alpha(s, 30, 30) == 0); // interior untouched
    CHECK(alpha(s, 4, 4) == 0);   // outside the rounded corner
    const double perimeter = 2.0 * ((52 - 16) + (52 - 16)) + 2.0 * kPi * 8.0;
    CHECK(near_rel(area(s), perimeter * 1.5, 0.06));
    // Mirror symmetry of the ring.
    for (int y = 0; y < 52; ++y) CHECK(alpha(s, 4 + y, 4) == alpha(s, 4 + y, 4 + 51) && alpha(s, 4, 4 + y) == alpha(s, 4 + 51, 4 + y));
    std::cout << "test_stroke_rounded passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Circles and lines
// ---------------------------------------------------------------------------------------------------------
void test_circle() {
    SoftwareSurface s(41, 41);
    { Painter p(s); p.fill_circle(20.5f, 20.5f, 10.0f, kWhite); }
    CHECK(alpha(s, 20, 20) == 255);
    CHECK(alpha(s, 20, 5) == 0 && alpha(s, 5, 20) == 0);
    CHECK(near_rel(area(s), kPi * 100.0, 0.01));
    for (int y = 0; y < 41; ++y) for (int x = 0; x < 41; ++x) CHECK(alpha(s, x, y) == alpha(s, 40 - x, y) && alpha(s, x, y) == alpha(s, x, 40 - y));

    // Centre on a pixel corner: the four surrounding pixels are identical.
    SoftwareSurface c(20, 20);
    { Painter p(c); p.fill_circle(10.0f, 10.0f, 4.0f, kWhite); }
    CHECK(alpha(c, 9, 9) == alpha(c, 10, 9) && alpha(c, 9, 9) == alpha(c, 9, 10) && alpha(c, 9, 9) == alpha(c, 10, 10));

    // Degenerate input draws nothing and does not crash.
    SoftwareSurface d(20, 20);
    {
        Painter p(d);
        p.fill_circle(10, 10, 0.0f, kWhite);
        p.fill_circle(10, 10, -5.0f, kWhite);
        p.fill_circle(std::nanf(""), 10, 5.0f, kWhite);
        p.fill_circle(10, std::numeric_limits<float>::infinity(), 5.0f, kWhite);
        p.fill_circle(10, 10, std::numeric_limits<float>::infinity(), kWhite);
        p.fill_circle(1e9f, 1e9f, 5.0f, kWhite);
        p.fill_circle(10, 10, 5.0f, Color{1, 2, 3, 0});
    }
    CHECK(count_alpha_nonzero(d) == 0);

    // A huge circle that covers the surface completes quickly and fills it.
    SoftwareSurface big(20, 20);
    { Painter p(big); p.fill_circle(10, 10, 1e7f, kWhite); }
    CHECK(count_alpha_nonzero(big) == 400);
    std::cout << "test_circle passed\n";
}

void test_stroke_circle() {
    SoftwareSurface s(41, 41);
    { Painter p(s); p.stroke_circle(20.5f, 20.5f, 10.0f, 2.0f, kWhite); }
    CHECK(alpha(s, 20, 20) == 0); // hole
    CHECK(near_rel(area(s), kPi * (100.0 - 64.0), 0.03));
    SoftwareSurface t(41, 41);
    { Painter p(t); p.stroke_circle(20.5f, 20.5f, 10.0f, 500.0f, kWhite); } // thicker than radius: solid disc
    CHECK(near_rel(area(t), kPi * 100.0, 0.01));
    std::cout << "test_stroke_circle passed\n";
}

void test_line() {
    {
        SoftwareSurface s(40, 20);
        { Painter p(s); p.draw_line(2.0f, 10.5f, 30.0f, 10.5f, 1.0f, kWhite); }
        for (int x = 3; x < 29; ++x) CHECK(alpha(s, x, 10) == 255);
        for (int x = 0; x < 40; ++x) CHECK(alpha(s, x, 9) == 0 && alpha(s, x, 11) == 0);
        CHECK(alpha(s, 1, 10) > 0 && alpha(s, 1, 10) < 255); // round cap, anti-aliased
        CHECK(alpha(s, 0, 10) == 0 && alpha(s, 31, 10) == 0);
    }
    {
        SoftwareSurface s(40, 20);
        { Painter p(s); p.draw_line(2.0f, 10.0f, 30.0f, 10.0f, 2.0f, kWhite); }
        CHECK(alpha(s, 15, 9) == 255 && alpha(s, 15, 10) == 255 && alpha(s, 15, 8) == 0 && alpha(s, 15, 11) == 0);
    }
    {
        SoftwareSurface s(30, 30);
        { Painter p(s); p.draw_line(2.0f, 2.0f, 17.0f, 17.0f, 1.0f, kWhite); }
        CHECK(alpha(s, 9, 9) == 255);
        for (int y = 0; y < 30; ++y) for (int x = 0; x < 30; ++x) CHECK(alpha(s, x, y) == alpha(s, y, x)); // symmetric under transpose
    }
    {
        SoftwareSurface s(30, 30);
        { Painter p(s); p.draw_line(15.0f, 15.0f, 15.0f, 15.0f, 4.0f, kWhite); } // zero length -> round dot
        CHECK(near_rel(area(s), kPi * 4.0, 0.06));
    }
    {
        SoftwareSurface s(40, 20); // a line that is enormous in both directions is clipped, not iterated
        { Painter p(s); p.draw_line(-1e6f, 10.5f, 1e6f, 10.5f, 1.0f, kWhite); }
        size_t full = 0;
        for (int x = 0; x < 40; ++x) full += (alpha(s, x, 10) == 255);
        CHECK(full == 40);
    }
    {
        SoftwareSurface s(20, 20);
        {
            Painter p(s);
            p.draw_line(1, 1, 18, 18, 0.0f, kWhite);
            p.draw_line(1, 1, 18, 18, -3.0f, kWhite);
            p.draw_line(std::nanf(""), 1, 18, 18, 2.0f, kWhite);
            p.draw_line(1, 1, std::numeric_limits<float>::infinity(), 18, 2.0f, kWhite);
            p.draw_line(1, 1, 18, 18, std::nanf(""), kWhite);
        }
        CHECK(count_alpha_nonzero(s) == 0);
    }
    std::cout << "test_line passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Gradients
// ---------------------------------------------------------------------------------------------------------
void test_linear_gradient() {
    const LinearGradient g_plain{Color{255, 255, 255, 255}, Color{0, 0, 0, 255}, true, false};
    SoftwareSurface s(8, 64);
    { Painter p(s); p.fill_gradient({0, 0, 8, 64}, g_plain); }
    CHECK(get(s, 3, 0).r >= 252);
    CHECK(get(s, 3, 63).r <= 3);
    CHECK(near(get(s, 3, 32).r, 127, 3));
    int prev = 256;
    for (int y = 0; y < 64; ++y) {
        const Px p = get(s, 3, y);
        CHECK(p.r == p.g && p.g == p.b && p.a == 255);
        CHECK(p.r <= prev);
        prev = p.r;
        for (int x = 1; x < 8; ++x) CHECK(get(s, x, y) == get(s, 0, y)); // no horizontal variation without dither
    }

    // Dithering is deterministic, stays within one level of the plain gradient, and varies across a row.
    const LinearGradient g_dither{Color{255, 255, 255, 255}, Color{0, 0, 0, 255}, true, true};
    SoftwareSurface d1(8, 64), d2(8, 64);
    { Painter p(d1); p.fill_gradient({0, 0, 8, 64}, g_dither); }
    { Painter p(d2); p.fill_gradient({0, 0, 8, 64}, g_dither); }
    bool varies = false;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 8; ++x) {
            CHECK(get(d1, x, y) == get(d2, x, y));
            CHECK(near(get(d1, x, y).r, get(s, x, y).r, 1));
            if (get(d1, x, y).r != get(d1, 0, y).r) varies = true;
        }
    }
    CHECK(varies);

    // Horizontal direction.
    const LinearGradient g_h{Color{0, 0, 0, 255}, Color{255, 255, 255, 255}, false, false};
    SoftwareSurface h(64, 4);
    { Painter p(h); p.fill_gradient({0, 0, 64, 4}, g_h); }
    CHECK(get(h, 0, 1).r <= 3 && get(h, 63, 1).r >= 252);
    CHECK(get(h, 10, 0) == get(h, 10, 3));

    // A constant colour is never disturbed by dithering.
    const LinearGradient flat{Color{40, 50, 60, 255}, Color{40, 50, 60, 255}, true, true};
    SoftwareSurface f(8, 8);
    { Painter p(f); p.fill_gradient({0, 0, 8, 8}, flat); }
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) CHECK(get(f, x, y) == (Px{40, 50, 60, 255}));
    std::cout << "test_linear_gradient passed\n";
}

void test_gradient_to_transparent_has_no_grey_fringe() {
    const LinearGradient g{Color{255, 255, 255, 255}, Color{255, 255, 255, 0}, true, false};
    SoftwareSurface s(4, 64);
    { Painter p(s); p.fill_gradient({0, 0, 4, 64}, g); }
    for (int y = 0; y < 64; ++y) {
        const Px p = get(s, 1, y);
        if (p.a > 0) CHECK(p.r >= 254 && p.g >= 254 && p.b >= 254); // colour is preserved while alpha fades
    }
    CHECK(near(get(s, 1, 32).a, 127, 4));
    CHECK(get(s, 1, 0).a >= 250 && get(s, 1, 63).a <= 4);
    std::cout << "test_gradient_to_transparent_has_no_grey_fringe passed\n";
}

void test_rounded_gradient() {
    const LinearGradient g{Color{200, 0, 0, 255}, Color{0, 0, 200, 255}, true, false};
    SoftwareSurface s(40, 40);
    { Painter p(s); p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::uniform(8.0f), g); }
    CHECK(alpha(s, 4, 4) == 0);
    CHECK(get(s, 20, 5).r > 180 && get(s, 20, 5).b < 20);
    CHECK(get(s, 20, 34).b > 180 && get(s, 20, 34).r < 20);
    std::cout << "test_rounded_gradient passed\n";
}

void test_radial_gradient() {
    SoftwareSurface s(41, 41);
    { Painter p(s); p.fill_radial_gradient(20.5f, 20.5f, 20.0f, Color{255, 255, 255, 255}, Color{255, 255, 255, 0}, false); }
    CHECK(alpha(s, 20, 20) >= 250);
    CHECK(alpha(s, 0, 0) == 0 && alpha(s, 40, 40) == 0); // beyond the radius: untouched
    CHECK(near(alpha(s, 30, 20), 134, 8));                // dist 9.5 -> ~52% of the way out
    int prev = 256;
    for (int x = 20; x < 41; ++x) {
        const int a = alpha(s, x, 20);
        CHECK(a <= prev);
        prev = a;
        if (a > 0) CHECK(get(s, x, 20).r >= 254);
    }
    std::cout << "test_radial_gradient passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------------------------------------
void draw_everything(Painter& p) {
    p.fill_rect({0, 0, 30, 30}, kWhite);
    p.fill_rounded_rect({0, 0, 30, 30}, CornerRadii::uniform(6.0f), kRed);
    p.fill_gradient({0, 0, 30, 30}, LinearGradient{kWhite, kRed, true, true});
    p.stroke_rounded_rect({0, 0, 30, 30}, CornerRadii::uniform(6.0f), 3.0f, kWhite);
    p.fill_circle(15.0f, 15.0f, 14.0f, kWhite);
    p.stroke_circle(15.0f, 15.0f, 14.0f, 3.0f, kWhite);
    p.draw_line(0.0f, 0.0f, 30.0f, 30.0f, 3.0f, kWhite);
    p.fill_radial_gradient(15.0f, 15.0f, 20.0f, kWhite, Color{255, 255, 255, 0});
    p.draw_shadow({8, 8, 14, 14}, CornerRadii::uniform(4.0f), ShadowSpec{6.0f, 0, 2, 0, Color{0, 0, 0, 200}});
    p.draw_box({0, 0, 30, 30}, CornerRadii::uniform(6.0f), kRed, 2.0f, kWhite);
}

void test_clipping() {
    SoftwareSurface s(30, 30);
    {
        Painter p(s);
        const Rect base = p.clip_bounds();
        CHECK(base.x == 0 && base.y == 0 && base.width == 30 && base.height == 30);
        CHECK(p.push_clip({10, 10, 10, 10}));
        draw_everything(p);
    }
    CHECK(count_alpha_nonzero_outside(s, 10, 10, 20, 20) == 0);
    CHECK(count_alpha_nonzero(s) > 50);

    // Nested clips intersect; popping restores the outer clip.
    SoftwareSurface n(30, 30);
    {
        Painter p(n);
        CHECK(p.push_clip({10, 10, 10, 10}));
        CHECK(p.push_clip({15, 15, 20, 20}));
        const Rect inner = p.clip_bounds();
        CHECK(inner.x == 15 && inner.y == 15 && inner.width == 5 && inner.height == 5);
        p.fill_rect({0, 0, 30, 30}, kWhite);
        p.pop_clip();
        const Rect outer = p.clip_bounds();
        CHECK(outer.x == 10 && outer.width == 10);
        p.fill_rect({0, 0, 30, 30}, kRed); // now fills the 10x10 clip (over the inner 5x5 white)
        p.pop_clip();
        p.pop_clip(); // popping the base clip is a harmless no-op
        CHECK(p.clip_bounds().width == 30);
    }
    CHECK(count_alpha_nonzero(n) == 100);
    CHECK(get(n, 17, 17) == (Px{255, 0, 0, 255}));

    // Empty clip: nothing is drawn.
    SoftwareSurface e(30, 30);
    {
        Painter p(e);
        CHECK(p.push_clip({100, 100, 5, 5})); // disjoint from the surface
        draw_everything(p);
    }
    CHECK(count_alpha_nonzero(e) == 0);

    // A clip that extends past the surface is trimmed to it.
    SoftwareSurface t(30, 30);
    {
        Painter p(t);
        CHECK(p.push_clip({-10, -10, 25, 25}));
        const Rect c = p.clip_bounds();
        CHECK(c.x == 0 && c.y == 0 && c.width == 15 && c.height == 15);
    }

    // Stack depth is bounded; ClipScope pops exactly what it pushed.
    SoftwareSurface d(30, 30);
    {
        Painter p(d);
        size_t pushed = 0;
        while (p.push_clip({0, 0, 30, 30})) ++pushed;
        CHECK(pushed == Painter::kMaxClipDepth - 1);
        {
            Painter::ClipScope scope(p, {0, 0, 5, 5});
            CHECK(!scope.pushed());
        }
        for (size_t i = 0; i < pushed; ++i) p.pop_clip();
        {
            Painter::ClipScope scope(p, {0, 0, 5, 5});
            CHECK(scope.pushed());
            p.fill_rect({0, 0, 30, 30}, kWhite);
        }
        CHECK(p.clip_bounds().width == 30);
    }
    CHECK(count_alpha_nonzero(d) == 25);
    std::cout << "test_clipping passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Shadows
// ---------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------
// Coverage masks (glyph bitmaps)
// ---------------------------------------------------------------------------------------------------------
void test_coverage_mask() {
    // Exact values: 0 leaves the pixel alone, 255 is the colour, in-between scales the alpha.
    {
        const uint8_t cov[6] = {0, 128, 255, 255, 0, 64};
        SoftwareSurface s(6, 4);
        { Painter p(s); p.draw_coverage_mask(1, 1, 3, 2, cov, 3, kWhite); }
        CHECK(get(s, 1, 1) == (Px{0, 0, 0, 0}));
        CHECK(get(s, 2, 1) == (Px{255, 255, 255, 128}));
        CHECK(get(s, 3, 1) == (Px{255, 255, 255, 255}));
        CHECK(get(s, 1, 2) == (Px{255, 255, 255, 255}));
        CHECK(get(s, 2, 2) == (Px{0, 0, 0, 0}));
        CHECK(get(s, 3, 2) == (Px{255, 255, 255, 64}));
        CHECK(count_alpha_nonzero(s) == 4);
    }
    // A translucent colour multiplies with the coverage; onto an opaque background it blends.
    {
        const uint8_t cov[2] = {255, 128};
        SoftwareSurface s(4, 2);
        {
            Painter p(s);
            p.fill_rect({0, 0, 4, 2}, Color{0, 0, 255, 255});
            p.draw_coverage_mask(0, 0, 2, 1, cov, 2, Color{255, 0, 0, 128});
        }
        const Px a = get(s, 0, 0);
        const Px b = get(s, 1, 0);
        CHECK(a.a == 255 && near(a.r, 128, 1) && near(a.b, 127, 1)); // alpha 128 over blue
        CHECK(b.a == 255 && near(b.r, 64, 1) && near(b.b, 191, 1));  // alpha 64 over blue
    }
    // Stride larger than the width: the gap bytes are never read.
    {
        // 2 cells wide, stride 5, 2 rows. Real cells are 10,20 / 30,40; the gap bytes are 99 and must never be drawn.
        const uint8_t cov[10] = {10, 20, 99, 99, 99, 30, 40, 99, 99, 99};
        SoftwareSurface s(5, 3);
        { Painter p(s); p.draw_coverage_mask(0, 0, 2, 2, cov, 5, kWhite); }
        CHECK(alpha(s, 0, 0) == 10 && alpha(s, 1, 0) == 20 && alpha(s, 0, 1) == 30 && alpha(s, 1, 1) == 40);
        CHECK(count_alpha_nonzero(s) == 4); // nothing from the gaps leaked in
        CHECK(alpha(s, 2, 0) == 0 && alpha(s, 2, 1) == 0);
    }
    // Clipping to the surface (origin partly outside) and to the clip stack: rows and columns stay aligned.
    {
        const uint8_t cov[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        SoftwareSurface s(4, 4);
        { Painter p(s); p.draw_coverage_mask(-1, -1, 3, 3, cov, 3, kWhite); } // only mask cells (1,1)..(2,2) land on the surface
        CHECK(alpha(s, 0, 0) == 5 && alpha(s, 1, 0) == 6 && alpha(s, 0, 1) == 8 && alpha(s, 1, 1) == 9);
        CHECK(count_alpha_nonzero(s) == 4);

        SoftwareSurface c(6, 6);
        {
            Painter p(c);
            CHECK(p.push_clip({2, 2, 2, 2}));
            p.draw_coverage_mask(0, 0, 6, 1, cov, 9, kWhite); // row outside the clip: nothing
            const uint8_t big[16] = {200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200};
            p.draw_coverage_mask(1, 1, 4, 4, big, 4, kWhite);
        }
        CHECK(count_alpha_nonzero(c) == 4);
        CHECK(alpha(c, 2, 2) == 200 && alpha(c, 3, 3) == 200 && alpha(c, 1, 1) == 0 && alpha(c, 4, 4) == 0);
    }
    // Invalid arguments draw nothing.
    {
        const uint8_t cov[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
        SoftwareSurface s(8, 8);
        {
            Painter p(s);
            p.draw_coverage_mask(0, 0, 4, 4, nullptr, 4, kWhite);
            p.draw_coverage_mask(0, 0, 0, 4, cov, 4, kWhite);
            p.draw_coverage_mask(0, 0, 4, 0, cov, 4, kWhite);
            p.draw_coverage_mask(0, 0, 4, 4, cov, 3, kWhite);      // stride < width
            p.draw_coverage_mask(0, 0, 4, 4, cov, 4, Color{255, 255, 255, 0});
            p.draw_coverage_mask(std::numeric_limits<int32_t>::min(), 0, 4, 4, cov, 4, kWhite);
            p.draw_coverage_mask(0, std::numeric_limits<int32_t>::max(), 4, 4, cov, 4, kWhite);
            p.draw_coverage_mask(100, 100, 4, 4, cov, 4, kWhite);   // entirely off-surface
        }
        CHECK(count_alpha_nonzero(s) == 0);
    }
    // Randomised comparison against a reference built from the (already tested) fill_rect, with poison bytes in the
    // stride gaps and after the mask: any over-read would put visible pixels where the reference has none.
    {
        uint64_t state = 0xBEEF;
        auto next = [&] {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<unsigned>(state >> 33);
        };
        for (int iter = 0; iter < 500; ++iter) {
            const int x = static_cast<int>(next() % 120) - 60;
            const int y = static_cast<int>(next() % 120) - 60;
            const uint32_t w = next() % 41;
            const uint32_t h = next() % 41;
            const size_t stride = w + next() % 11;
            std::vector<uint8_t> mask(stride * (h > 0 ? h : 1) + 64, 255); // poison everywhere...
            for (uint32_t r = 0; r < h; ++r) {
                for (uint32_t c = 0; c < w; ++c) mask[r * stride + c] = static_cast<uint8_t>(next() % 5 == 0 ? 0 : next()); // ...except real cells
            }
            const Color color{static_cast<uint8_t>(next()), static_cast<uint8_t>(next()), static_cast<uint8_t>(next()), static_cast<uint8_t>(next() % 3 == 0 ? 255 : next())};
            const bool use_clip = next() % 3 == 0;
            const Rect clip{static_cast<int32_t>(next() % 60) - 10, static_cast<int32_t>(next() % 60) - 10, next() % 50, next() % 50};

            SoftwareSurface actual(40, 40), expected(40, 40);
            {
                Painter pa(actual);
                Painter pe(expected);
                // Identical opaque backgrounds, so blending (not only coverage) is compared.
                pa.fill_rect({0, 0, 40, 40}, Color{20, 40, 60, 255});
                pe.fill_rect({0, 0, 40, 40}, Color{20, 40, 60, 255});
                if (use_clip) {
                    pa.push_clip(clip);
                    pe.push_clip(clip);
                }
                pa.draw_coverage_mask(x, y, w, h, mask.data(), stride, color);
                for (uint32_t r = 0; r < h; ++r) {
                    for (uint32_t c = 0; c < w; ++c) {
                        const unsigned cov = mask[r * stride + c];
                        if (cov == 0) continue;
                        const unsigned a = cov == 255 ? color.a : (color.a * cov + 127) / 255;
                        pe.fill_rect({x + static_cast<int>(c), y + static_cast<int>(r), 1, 1}, Color{color.r, color.g, color.b, static_cast<uint8_t>(a)});
                    }
                }
            }
            // Every byte of every pixel must agree (blending rounding is the same code path, so +-1 is generous).
            const auto pa = actual.map_pixels();
            const auto pe = expected.map_pixels();
            bool same = pa.size() == pe.size();
            for (size_t i = 0; same && i < pa.size(); ++i) same = std::abs(int{pa[i]} - int{pe[i]}) <= 1;
            actual.unmap_pixels();
            expected.unmap_pixels();
            CHECK(same);
        }
    }
    std::cout << "test_coverage_mask passed\n";
}

void test_shadow() {
    const ShadowSpec spec{12.0f, 0.0f, 6.0f, 0.0f, Color{0, 0, 0, 200}};
    const Rect box{30, 30, 60, 40};

    SoftwareSurface s(120, 100);
    { Painter p(s); p.draw_shadow(box, CornerRadii::uniform(8.0f), spec); }
    CHECK(alpha(s, 60, 50) == 0);  // cut out: the box interior is untouched
    CHECK(alpha(s, 0, 0) == 0);    // far away
    CHECK(alpha(s, 10, 50) == 0);  // beyond the blur band on the left
    int prev = 256;
    int peak = 0;
    for (int y = 70; y < 100; ++y) { // walking away from the box's lower edge
        const int a = alpha(s, 60, y);
        CHECK(a <= prev);
        CHECK(a <= 200);
        peak = std::max(peak, a);
        prev = a;
    }
    CHECK(peak > 120);              // visible just below the box
    CHECK(alpha(s, 60, 99) == 0);   // faded out by the bottom
    // Offset (0, +6) pushes the shadow down: more mass below the box than above it.
    double below = 0, above = 0;
    for (int x = 30; x < 90; ++x) {
        for (int y = 70; y < 100; ++y) below += alpha(s, x, y);
        for (int y = 0; y < 30; ++y) above += alpha(s, x, y);
    }
    CHECK(below > above * 1.5);

    // Without the cut-out, the interior carries the shadow too.
    SoftwareSurface filled(120, 100);
    { Painter p(filled); p.draw_shadow(box, CornerRadii::uniform(8.0f), spec, false); }
    CHECK(alpha(filled, 60, 50) >= 190);

    // With no offset the shadow is mirror-symmetric (within one level of float error).
    SoftwareSurface sym(120, 100);
    { Painter p(sym); p.draw_shadow(box, CornerRadii::uniform(8.0f), ShadowSpec{12.0f, 0, 0, 0, Color{0, 0, 0, 200}}); }
    for (int y = 0; y < 100; ++y) for (int x = 0; x < 120; ++x) CHECK(near(alpha(sym, x, y), alpha(sym, 119 - x, y), 1));

    // Hard shadow (blur 0) with an offset leaves an L-shaped sliver of exactly the peak alpha.
    SoftwareSurface hard(120, 100);
    { Painter p(hard); p.draw_shadow(box, CornerRadii{}, ShadowSpec{0.0f, 4, 4, 0, Color{0, 0, 0, 200}}); }
    CHECK(alpha(hard, 92, 50) == 200 && alpha(hard, 94, 50) == 0); // right sliver: x in [90,94)
    CHECK(alpha(hard, 60, 72) == 200 && alpha(hard, 60, 74) == 0); // bottom sliver
    CHECK(alpha(hard, 60, 50) == 0);

    // Spread grows the shape.
    SoftwareSurface spread(120, 100);
    { Painter p(spread); p.draw_shadow(box, CornerRadii{}, ShadowSpec{0.0f, 0, 0, 4, Color{0, 0, 0, 200}}); }
    CHECK(alpha(spread, 92, 50) == 200 && alpha(spread, 94, 50) == 0);

    // Degenerate input: no change, no crash.
    SoftwareSurface d(60, 60);
    {
        Painter p(d);
        p.draw_shadow({10, 10, 20, 20}, CornerRadii{}, ShadowSpec{8, 0, 0, 0, Color{0, 0, 0, 0}});
        p.draw_shadow({10, 10, 20, 20}, CornerRadii{}, ShadowSpec{std::nanf(""), 0, 0, 0, Color{0, 0, 0, 200}});
        p.draw_shadow({10, 10, 0, 20}, CornerRadii{}, ShadowSpec{8, 0, 0, 0, Color{0, 0, 0, 200}});
        p.draw_shadow({10, 10, 20, 20}, CornerRadii{}, ShadowSpec{8, 0, 0, -1000, Color{0, 0, 0, 200}});
        p.draw_shadow({10, 10, 20, 20}, CornerRadii{}, ShadowSpec{1e30f, 0, 0, 0, Color{0, 0, 0, 200}});
    }
    std::cout << "test_shadow passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Bounds safety (guard bytes + randomised fuzz) and surfaces the painter must refuse
// ---------------------------------------------------------------------------------------------------------
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
    bool guards_intact() const {
        for (size_t i = 0; i < kGuard; ++i) {
            if (m_store[i] != 0xAB || m_store[m_store.size() - 1 - i] != 0xAB) return false;
        }
        return true;
    }

private:
    uint32_t m_w, m_h;
    std::vector<uint8_t> m_store;
};

class LyingSurface final : public ISurface { // reports a size larger than the buffer it maps
public:
    uint32_t width() const override { return 100; }
    uint32_t height() const override { return 100; }
    std::span<uint8_t> map_pixels() override { return std::span<uint8_t>(m_buf, sizeof(m_buf)); }
    void unmap_pixels() override {}
    uint8_t m_buf[16]{};
};

struct Rng {
    uint64_t s;
    uint32_t next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<uint32_t>(s >> 33);
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1)); }
    float f(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000) / 100000.0f); }
    float wild() {
        switch (next() % 12) {
        case 0: return std::nanf("");
        case 1: return std::numeric_limits<float>::infinity();
        case 2: return -std::numeric_limits<float>::infinity();
        case 3: return 1e30f;
        case 4: return -1e30f;
        case 5: return 0.0f;
        default: return f(-300.0f, 300.0f);
        }
    }
    uint32_t dim() {
        switch (next() % 10) {
        case 0: return 0;
        case 1: return std::numeric_limits<uint32_t>::max();
        case 2: return 0x7FFFFFFFu;
        default: return static_cast<uint32_t>(range(0, 300));
        }
    }
    int32_t coord() {
        switch (next() % 10) {
        case 0: return std::numeric_limits<int32_t>::min();
        case 1: return std::numeric_limits<int32_t>::max();
        default: return range(-250, 250);
        }
    }
    Color color() { return Color{static_cast<uint8_t>(next()), static_cast<uint8_t>(next()), static_cast<uint8_t>(next()), static_cast<uint8_t>(next())}; }
};

void fuzz_painter(GuardedSurface& surface, uint64_t seed, int iterations) {
    Rng rng{seed};
    Painter p(surface);
    for (int i = 0; i < iterations; ++i) {
        const Rect r{rng.coord(), rng.coord(), rng.dim(), rng.dim()};
        const CornerRadii radii{rng.wild(), rng.wild(), rng.wild(), rng.wild()};
        const Color c = rng.color();
        switch (rng.next() % 12) {
        case 0: p.fill_rect(r, c); break;
        case 1: p.fill_rounded_rect(r, radii, c); break;
        case 2: p.fill_rounded_rect(r, radii, LinearGradient{rng.color(), rng.color(), (rng.next() & 1) != 0, (rng.next() & 1) != 0}); break;
        case 3: p.stroke_rounded_rect(r, radii, rng.wild(), c); break;
        case 4: p.fill_circle(rng.wild(), rng.wild(), rng.wild(), c); break;
        case 5: p.stroke_circle(rng.wild(), rng.wild(), rng.wild(), rng.wild(), c); break;
        case 6: p.draw_line(rng.wild(), rng.wild(), rng.wild(), rng.wild(), rng.wild(), c); break;
        case 7: p.fill_radial_gradient(rng.wild(), rng.wild(), rng.wild(), c, rng.color()); break;
        case 8: p.draw_shadow(r, radii, ShadowSpec{rng.wild(), rng.wild(), rng.wild(), rng.wild(), c}, (rng.next() & 1) != 0); break;
        case 9: p.fill_gradient(r, LinearGradient{c, rng.color(), (rng.next() & 1) != 0, true}); break;
        case 10: p.push_clip(r); break;
        default: p.pop_clip(); break;
        }
        if (!surface.guards_intact()) {
            std::cout << "  guard bytes corrupted at iteration " << i << " (op " << rng.next() % 12 << ")\n";
            CHECK(false);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------
// draw_box: a fill and its border from ONE rectangle
// ---------------------------------------------------------------------------------------------------------
std::vector<uint8_t> snapshot(SoftwareSurface& s) {
    auto m = s.map_pixels();
    std::vector<uint8_t> copy(m.begin(), m.end());
    s.unmap_pixels();
    return copy;
}

constexpr Color kBoxFill{0, 0, 200, 255};   // no red, no green
constexpr Color kBoxBorder{0, 200, 0, 255}; // no red, no blue
constexpr Px kRedPx{255, 0, 0, 255};

void test_draw_box_square_is_exact() {
    for (const int thickness : {1, 2, 3}) {
        SoftwareSurface s(40, 30);
        {
            Painter p(s);
            p.fill_rect({0, 0, 40, 30}, kRed);
            p.draw_box({4, 3, 30, 20}, CornerRadii{}, kBoxFill, static_cast<float>(thickness), kBoxBorder);
        }
        // Every pixel of the rectangle is exactly the border (within `thickness` of an edge) or exactly the fill, and
        // every pixel outside it is untouched: the border and the fill tile the rectangle with nothing in between.
        size_t border_pixels = 0, fill_pixels = 0;
        for (int y = 0; y < 30; ++y) {
            for (int x = 0; x < 40; ++x) {
                const Px px = get(s, x, y);
                const bool inside = x >= 4 && x < 34 && y >= 3 && y < 23;
                if (!inside) {
                    CHECK(px == kRedPx);
                    continue;
                }
                const bool on_border = x < 4 + thickness || x >= 34 - thickness || y < 3 + thickness || y >= 23 - thickness;
                CHECK(px == (on_border ? Px{0, 200, 0, 255} : Px{0, 0, 200, 255}));
                (on_border ? border_pixels : fill_pixels) += 1;
            }
        }
        CHECK(fill_pixels == static_cast<size_t>((30 - 2 * thickness) * (20 - 2 * thickness)));
        CHECK(border_pixels + fill_pixels == 30u * 20u);
    }
    std::cout << "test_draw_box_square_is_exact passed\n";
}

void test_draw_box_has_no_gap_between_fill_and_border() {
    // Drawn over a red background with a fill and a border that contain no red: any red inside the shape would be
    // background showing through, i.e. a gap. The reference coverage of the shape is a white fill of the same rectangle
    // and radii on a transparent surface.
    const Rect rect{6, 5, 48, 38};
    for (const float radius : {0.0f, 3.0f, 6.0f, 8.0f, 10.0f, 19.0f, 40.0f}) {
        for (const float thickness : {1.0f, 1.5f, 2.0f, 3.0f}) {
            SoftwareSurface coverage(60, 50);
            { Painter p(coverage); p.fill_rounded_rect(rect, CornerRadii::uniform(radius), kWhite); }
            SoftwareSurface s(60, 50);
            {
                Painter p(s);
                p.fill_rect({0, 0, 60, 50}, kRed);
                p.draw_box(rect, CornerRadii::uniform(radius), kBoxFill, thickness, kBoxBorder);
            }
            for (int y = 0; y < 50; ++y) {
                for (int x = 0; x < 60; ++x) {
                    const int cov = alpha(coverage, x, y);
                    const Px px = get(s, x, y);
                    if (cov == 0) {
                        CHECK(px == kRedPx); // outside the shape: untouched
                    } else if (cov == 255) {
                        CHECK(px.r == 0 && px.a == 255); // inside the shape: no background at all
                    } else {
                        // The anti-aliased outer edge: exactly the shape's coverage of border over background...
                        CHECK(near(px.r, 255 - cov, 2));
                        // ...and it is the BORDER that meets the background there, never a fringe of fill colour
                        // (a separate fill-then-stroke leaves up to a quarter of the fill in these pixels).
                        if (thickness >= 2.0f) CHECK(px.b <= 2);
                        CHECK(near(px.g, 200 * cov / 255, 3));
                    }
                }
            }
        }
    }
    std::cout << "test_draw_box_has_no_gap_between_fill_and_border passed\n";
}

void test_draw_box_shares_the_fill_geometry() {
    // With the border the same colour as the fill, a box is pixel-for-pixel the plain rounded fill: the border adds
    // nothing outside the fill's shape and removes nothing inside it, whatever the radius, thickness or size.
    const Color c{40, 90, 160, 255};
    const Rect rects[] = {{5, 5, 50, 40}, {0, 0, 64, 48}, {10, 12, 1, 1}, {10, 12, 2, 30}, {10, 12, 30, 2}, {-8, -6, 40, 30}, {40, 30, 60, 50}};
    for (const Rect& rect : rects) {
        for (const float radius : {0.0f, 4.0f, 6.0f, 10.0f, 100.0f}) {
            for (const float thickness : {0.5f, 1.0f, 2.0f, 7.0f}) {
                SoftwareSurface plain(64, 48), boxed(64, 48);
                { Painter p(plain); p.fill_rounded_rect(rect, CornerRadii::uniform(radius), c); }
                { Painter p(boxed); p.draw_box(rect, CornerRadii::uniform(radius), c, thickness, c); }
                CHECK(snapshot(plain) == snapshot(boxed));
            }
        }
    }
    // Per-corner radii too (a tab: rounded top, square bottom).
    SoftwareSurface plain(64, 48), boxed(64, 48);
    { Painter p(plain); p.fill_rounded_rect({8, 8, 40, 30}, CornerRadii::top(6.0f), c); }
    { Painter p(boxed); p.draw_box({8, 8, 40, 30}, CornerRadii::top(6.0f), c, 1.0f, c); }
    CHECK(snapshot(plain) == snapshot(boxed));
    std::cout << "test_draw_box_shares_the_fill_geometry passed\n";
}

void test_draw_box_rounded_ring_and_interior() {
    SoftwareSurface s(80, 60);
    const Rect rect{10, 8, 60, 44};
    { Painter p(s); p.draw_box(rect, CornerRadii::uniform(8.0f), kBoxFill, 2.0f, kBoxBorder); }
    // Straight edges: the two outermost pixels are border, the third is fill.
    CHECK(get(s, 40, 8) == Px{0, 200, 0, 255} && get(s, 40, 9) == Px{0, 200, 0, 255} && get(s, 40, 10) == Px{0, 0, 200, 255});
    CHECK(get(s, 10, 30) == Px{0, 200, 0, 255} && get(s, 11, 30) == Px{0, 200, 0, 255} && get(s, 12, 30) == Px{0, 0, 200, 255});
    CHECK(get(s, 69, 30) == Px{0, 200, 0, 255} && get(s, 67, 30) == Px{0, 0, 200, 255});
    CHECK(get(s, 40, 51) == Px{0, 200, 0, 255} && get(s, 40, 49) == Px{0, 0, 200, 255});
    CHECK(get(s, 40, 30) == Px{0, 0, 200, 255}); // the middle
    // The corner square is transparent at its tip, and nothing is drawn outside the rectangle.
    CHECK(alpha(s, 10, 8) == 0 && alpha(s, 69, 51) == 0);
    CHECK(count_alpha_nonzero_outside(s, 10, 8, 70, 52) == 0);
    // The ring follows the corner: along the diagonal, border comes before fill.
    bool seen_border = false, seen_fill = false, fill_before_border = false;
    for (int d = 0; d < 14; ++d) {
        const Px px = get(s, 10 + d, 8 + d);
        if (px.a == 0) continue;
        // On the diagonal a 2 px ring is only ever seen blended, so classify each pixel by what dominates it.
        if (px.g > px.b) {
            seen_border = true;
            if (seen_fill) fill_before_border = true;
        } else if (px.b > px.g) {
            seen_fill = true;
        }
    }
    CHECK(seen_border && seen_fill && !fill_before_border);
    // Mirror symmetry in both axes.
    for (int y = 0; y < 44; ++y) {
        for (int x = 0; x < 60; ++x) {
            CHECK(get(s, 10 + x, 8 + y) == get(s, 10 + 59 - x, 8 + y));
            CHECK(get(s, 10 + x, 8 + y) == get(s, 10 + x, 8 + 43 - y));
        }
    }
    // The border's area (each pixel's share of border, weighted by its coverage) is the outer rounded rectangle minus
    // the inner one: (60x44 with r=8) - (56x40 with r=6).
    double border_area = 0.0;
    for (int y = 0; y < 60; ++y) {
        for (int x = 0; x < 80; ++x) {
            const Px px = get(s, x, y);
            border_area += (px.g / 200.0) * (px.a / 255.0);
        }
    }
    const double outer_area = 60.0 * 44.0 - (4.0 - kPi) * 8.0 * 8.0;
    const double inner_area = 56.0 * 40.0 - (4.0 - kPi) * 6.0 * 6.0;
    CHECK(near_rel(border_area, outer_area - inner_area, 0.03));
    std::cout << "test_draw_box_rounded_ring_and_interior passed\n";
}

void test_draw_box_degenerate_arguments() {
    const Rect rect{6, 6, 40, 30};
    const CornerRadii radii = CornerRadii::uniform(6.0f);
    SoftwareSurface fill_only(60, 50);
    { Painter p(fill_only); p.fill_rounded_rect(rect, radii, kBoxFill); }
    const auto expected_fill = snapshot(fill_only);

    // No border: zero, negative or non-finite thickness, or a transparent border, draw the fill alone.
    for (const float thickness : {0.0f, -1.0f, -1e30f, std::nanf(""), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        SoftwareSurface s(60, 50);
        { Painter p(s); p.draw_box(rect, radii, kBoxFill, thickness, kBoxBorder); }
        CHECK(snapshot(s) == expected_fill);
    }
    {
        SoftwareSurface s(60, 50);
        { Painter p(s); p.draw_box(rect, radii, kBoxFill, 2.0f, Color{0, 200, 0, 0}); }
        CHECK(snapshot(s) == expected_fill);
    }
    // No fill: the border alone, identical to stroke_rounded_rect.
    {
        SoftwareSurface stroked(60, 50), boxed(60, 50);
        { Painter p(stroked); p.stroke_rounded_rect(rect, radii, 2.0f, kBoxBorder); }
        { Painter p(boxed); p.draw_box(rect, radii, Color{0, 0, 200, 0}, 2.0f, kBoxBorder); }
        CHECK(snapshot(stroked) == snapshot(boxed));
        CHECK(alpha(boxed, 26, 21) == 0); // hollow
    }
    // A border at least half as thick as the box covers all of it.
    {
        SoftwareSurface solid(60, 50);
        { Painter p(solid); p.fill_rounded_rect(rect, radii, kBoxBorder); }
        for (const float thickness : {15.0f, 20.0f, 500.0f, 1e30f}) {
            SoftwareSurface fresh(60, 50);
            { Painter p(fresh); p.draw_box(rect, radii, kBoxFill, thickness, kBoxBorder); }
            CHECK(snapshot(fresh) == snapshot(solid));
        }
    }
    // Empty, inverted-by-overflow, far-away and enormous rectangles draw nothing or what is visible, and never crash.
    {
        GuardedSurface s(32, 24);
        Painter p(s);
        p.draw_box({5, 5, 0, 10}, radii, kBoxFill, 1.0f, kBoxBorder);
        p.draw_box({5, 5, 10, 0}, radii, kBoxFill, 1.0f, kBoxBorder);
        p.draw_box({500, 500, 10, 10}, radii, kBoxFill, 1.0f, kBoxBorder);
        p.draw_box({-500, -500, 10, 10}, radii, kBoxFill, 1.0f, kBoxBorder);
        p.draw_box({std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(), 10, 10}, radii, kBoxFill, 1.0f, kBoxBorder);
        p.draw_box({std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(), 10, 10}, radii, kBoxFill, 1.0f, kBoxBorder);
        CHECK(s.guards_intact());
        auto m = s.map_pixels();
        CHECK(std::all_of(m.begin(), m.end(), [](uint8_t b) { return b == 0; })); // none of those is on the surface
        p.draw_box({-100, -100, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()}, CornerRadii::uniform(std::nanf("")),
                   kBoxFill, 1.0f, kBoxBorder);
        CHECK(s.guards_intact());
        // An enormous box covers the whole surface with its fill (its border is far off-screen).
        for (size_t i = 0; i < m.size(); i += 4) CHECK(m[i] == 0 && m[i + 1] == 0 && m[i + 2] == 200 && m[i + 3] == 255);
    }
    std::cout << "test_draw_box_degenerate_arguments passed\n";
}

void test_draw_box_clipping() {
    const Rect rect{4, 4, 50, 40};
    const CornerRadii radii = CornerRadii::uniform(8.0f);
    SoftwareSurface whole(60, 50);
    { Painter p(whole); p.draw_box(rect, radii, kBoxFill, 2.0f, kBoxBorder); }
    const auto reference = snapshot(whole);

    // Clipped drawing equals the unclipped drawing inside the clip and leaves everything outside it alone.
    const Rect clips[] = {{0, 0, 60, 50}, {10, 10, 20, 15}, {0, 0, 12, 12}, {40, 30, 30, 30}, {4, 4, 1, 40}, {20, 4, 30, 2}, {53, 43, 1, 1}};
    for (const Rect& clip : clips) {
        SoftwareSurface s(60, 50);
        {
            Painter p(s);
            const Painter::ClipScope scope(p, clip);
            CHECK(scope.pushed());
            p.draw_box(rect, radii, kBoxFill, 2.0f, kBoxBorder);
        }
        const auto clipped = snapshot(s);
        for (int y = 0; y < 50; ++y) {
            for (int x = 0; x < 60; ++x) {
                const bool in_clip = x >= clip.x && x < clip.x + static_cast<int>(clip.width) && y >= clip.y && y < clip.y + static_cast<int>(clip.height);
                const size_t i = (static_cast<size_t>(y) * 60 + static_cast<size_t>(x)) * 4;
                for (size_t ch = 0; ch < 4; ++ch) CHECK(clipped[i + ch] == (in_clip ? reference[i + ch] : 0));
            }
        }
    }
    // A box hanging off every edge of the surface draws exactly its visible part.
    SoftwareSurface big(100, 90), cropped(60, 50);
    { Painter p(big); p.draw_box({10, 10, 80, 70}, radii, kBoxFill, 2.0f, kBoxBorder); }
    { Painter p(cropped); p.draw_box({-10, -10, 80, 70}, radii, kBoxFill, 2.0f, kBoxBorder); }
    const auto big_pixels = snapshot(big);
    const auto cropped_pixels = snapshot(cropped);
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 60; ++x) {
            const size_t a = (static_cast<size_t>(y + 20) * 100 + static_cast<size_t>(x + 20)) * 4;
            const size_t b = (static_cast<size_t>(y) * 60 + static_cast<size_t>(x)) * 4;
            for (size_t ch = 0; ch < 4; ++ch) CHECK(big_pixels[a + ch] == cropped_pixels[b + ch]);
        }
    }
    std::cout << "test_draw_box_clipping passed\n";
}

void test_draw_box_translucent_colours() {
    // A translucent border tints the fill under it: the ring is exactly border-over-fill, the interior exactly the fill.
    const Color fill{30, 40, 50, 255};
    const Color border{34, 199, 220, 102}; // 40%
    SoftwareSurface s(40, 30);
    { Painter p(s); p.draw_box({4, 4, 30, 20}, CornerRadii{}, fill, 1.0f, border); }
    const Color ring = blend_over(border, fill);
    CHECK(get(s, 4, 10) == Px{ring.r, ring.g, ring.b, 255});
    CHECK(get(s, 20, 4) == Px{ring.r, ring.g, ring.b, 255});
    CHECK(get(s, 20, 14) == Px{30, 40, 50, 255});
    CHECK(ring.r != fill.r && ring.g > fill.g); // and it really is a tint, not either colour alone

    // A translucent fill keeps its alpha on a transparent surface; an opaque border over it is opaque.
    SoftwareSurface t(40, 30);
    { Painter p(t); p.draw_box({4, 4, 30, 20}, CornerRadii{}, Color{255, 255, 255, 0x12}, 1.0f, kBoxBorder); }
    CHECK(get(t, 20, 14) == Px{255, 255, 255, 0x12});
    CHECK(get(t, 4, 14) == Px{0, 200, 0, 255});
    CHECK(alpha(t, 3, 14) == 0);
    std::cout << "test_draw_box_translucent_colours passed\n";
}

void test_draw_box_fuzz() {
    const std::pair<uint32_t, uint32_t> sizes[] = {{37, 29}, {1, 1}, {1, 64}, {64, 1}, {96, 72}};
    uint64_t seed = 0xB0C5;
    for (const auto& [w, h] : sizes) {
        GuardedSurface s(w, h);
        Rng rng{seed++};
        Painter p(s);
        for (int i = 0; i < 3000; ++i) {
            const Rect r{rng.coord(), rng.coord(), rng.dim(), rng.dim()};
            const CornerRadii radii{rng.wild(), rng.wild(), rng.wild(), rng.wild()};
            switch (rng.next() % 8) {
            case 0: p.push_clip({rng.coord(), rng.coord(), rng.dim(), rng.dim()}); break;
            case 1: p.pop_clip(); break;
            default: p.draw_box(r, radii, rng.color(), rng.wild(), rng.color()); break;
            }
            if (!s.guards_intact()) {
                std::cout << "  guard bytes corrupted at draw_box iteration " << i << "\n";
                CHECK(false);
            }
        }
    }
    { GuardedSurface zero(0, 0); Painter p(zero); p.draw_box({0, 0, 10, 10}, CornerRadii::uniform(3.0f), kBoxFill, 1.0f, kBoxBorder); CHECK(zero.guards_intact()); }
    {
        LyingSurface liar;
        Painter p(liar);
        p.draw_box({0, 0, 100, 100}, CornerRadii::uniform(3.0f), kBoxFill, 1.0f, kBoxBorder);
        for (uint8_t b : liar.m_buf) CHECK(b == 0);
    }
    std::cout << "test_draw_box_fuzz passed\n";
}

void test_bounds_safety_fuzz() {
    const std::pair<uint32_t, uint32_t> sizes[] = {{37, 29}, {1, 1}, {1, 64}, {64, 1}, {128, 96}};
    uint64_t seed = 0x5EED;
    for (const auto& [w, h] : sizes) {
        for (int round = 0; round < 3; ++round) {
            GuardedSurface s(w, h);
            fuzz_painter(s, seed++, 1500);
            CHECK(s.guards_intact());
        }
    }

    // Surfaces the painter must refuse rather than trust.
    { GuardedSurface zero(0, 0); Painter p(zero); draw_everything(p); CHECK(zero.guards_intact()); }
    { GuardedSurface zero_w(0, 10); Painter p(zero_w); draw_everything(p); }
    {
        LyingSurface liar; // claims 100x100 but maps 16 bytes
        Painter p(liar);
        draw_everything(p);
        CHECK(p.width() == 0 && p.height() == 0);
        for (uint8_t b : liar.m_buf) CHECK(b == 0);
    }
    std::cout << "test_bounds_safety_fuzz passed\n";
}

void test_no_heap_allocation_while_drawing() {
    SoftwareSurface s(256, 256);
    // Warm up anything lazily allocated by the C++ runtime before counting.
    { Painter p(s); draw_everything(p); }

    // Prove the instrumentation works, otherwise "0 allocations" below would be meaningless.
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
    for (int i = 0; i < 50; ++i) {
        Painter p(s);
        Painter::ClipScope scope(p, {10, 10, 200, 200});
        p.fill_rect({0, 0, 256, 256}, Color{20, 24, 30, 255});
        p.fill_rounded_rect({20, 20, 200, 150}, CornerRadii::uniform(10.0f), Color{30, 36, 44, 255});
        p.fill_rounded_rect({20, 20, 200, 30}, CornerRadii::top(10.0f), LinearGradient{Color{44, 54, 66, 255}, Color{34, 42, 51, 255}, true, true});
        p.stroke_rounded_rect({20, 20, 200, 150}, CornerRadii::uniform(10.0f), 1.0f, Color{54, 65, 77, 255});
        p.fill_circle(200.0f, 35.0f, 6.0f, Color{229, 72, 77, 255});
        p.stroke_circle(180.0f, 35.0f, 6.0f, 1.5f, kWhite);
        p.draw_line(40.0f, 100.0f, 200.0f, 100.0f, 1.0f, Color{54, 65, 77, 255});
        p.draw_shadow({20, 20, 200, 150}, CornerRadii::uniform(10.0f), ShadowSpec{12.0f, 0, 4, 0, Color{0, 0, 0, 120}});
        p.fill_radial_gradient(128.0f, 128.0f, 100.0f, Color{34, 199, 220, 40}, Color{34, 199, 220, 0});
        p.draw_box({30, 30, 120, 32}, CornerRadii::uniform(6.0f), Color{34, 42, 51, 255}, 1.0f, Color{54, 65, 77, 255});
        p.draw_box({30, 70, 120, 32}, CornerRadii::top(6.0f), Color{34, 199, 220, 38}, 2.0f, Color{34, 199, 220, 176});
        p.draw_box({-20, 110, 400, 400}, CornerRadii::uniform(10.0f), Color{26, 32, 39, 255}, 1.5f, Color{40, 49, 58, 255});
    }
    g_count_allocs = false;
    if (g_alloc_count != 0) std::cout << "  unexpected heap allocations: " << g_alloc_count.load() << "\n";
    CHECK(g_alloc_count == 0);
    std::cout << "test_no_heap_allocation_while_drawing passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Toolkit integration and backwards compatibility
// ---------------------------------------------------------------------------------------------------------
void test_toolkit_draw_rect_compatibility() {
    BasicWidgetToolkit toolkit;

    // Opaque and alpha-over-opaque results match the legacy toolkit formula.
    auto surface = std::make_shared<SoftwareSurface>(20, 20);
    toolkit.draw_rect(surface, 0, 0, 20, 20, Color{200, 100, 50, 255});
    toolkit.draw_rect(surface, 2, 2, 10, 10, Color{10, 220, 90, 77});
    const float alpha_f = 77 / 255.0f;
    const Px blended = get(*surface, 5, 5);
    CHECK(near(blended.r, static_cast<int>(10 * alpha_f + 200 * (1 - alpha_f)), 1));
    CHECK(near(blended.g, static_cast<int>(220 * alpha_f + 100 * (1 - alpha_f)), 1));
    CHECK(near(blended.b, static_cast<int>(90 * alpha_f + 50 * (1 - alpha_f)), 1));
    CHECK(blended.a == 255);
    CHECK(get(*surface, 15, 15) == (Px{200, 100, 50, 255}));

    // Onto a transparent surface (decorations): the colour is no longer darkened.
    auto deco = std::make_shared<SoftwareSurface>(8, 8);
    toolkit.draw_rect(deco, 0, 0, 4, 4, Color{200, 100, 50, 128});
    CHECK(get(*deco, 1, 1) == (Px{200, 100, 50, 128}));

    // Wild arguments and null surfaces are safe.
    toolkit.draw_rect(nullptr, 0, 0, 5, 5, kWhite);
    toolkit.draw_rect(surface, -50, -50, 20, 20, kWhite);
    toolkit.draw_rect(surface, 10, 10, 0, 5, kWhite);
    toolkit.draw_rect(surface, 1000, 1000, 5, 5, kWhite);
    std::cout << "test_toolkit_draw_rect_compatibility passed\n";
}

void test_toolkit_extended_drawing() {
    BasicWidgetToolkit toolkit;
    auto s = std::make_shared<SoftwareSurface>(60, 60);
    toolkit.draw_rounded_rect(s, {4, 4, 52, 52}, CornerRadii::uniform(10.0f), kWhite);
    CHECK(alpha(*s, 4, 4) == 0 && alpha(*s, 30, 30) == 255);

    auto b = std::make_shared<SoftwareSurface>(60, 60);
    toolkit.draw_rounded_border(b, {4, 4, 52, 52}, CornerRadii::uniform(10.0f), 1.0f, kWhite);
    CHECK(alpha(*b, 30, 30) == 0 && alpha(*b, 30, 4) == 255);

    auto g = std::make_shared<SoftwareSurface>(60, 60);
    toolkit.draw_gradient_rect(g, {0, 0, 60, 60}, CornerRadii{}, LinearGradient{kWhite, Color{0, 0, 0, 255}, true, false});
    CHECK(get(*g, 5, 1).r > get(*g, 5, 58).r);

    auto sh = std::make_shared<SoftwareSurface>(100, 100);
    toolkit.draw_shadow(sh, {30, 30, 40, 40}, CornerRadii::uniform(6.0f), ShadowSpec{10, 0, 4, 0, Color{0, 0, 0, 180}});
    CHECK(alpha(*sh, 50, 50) == 0 && alpha(*sh, 50, 73) > 0);

    // The toolkit exposes the design tokens.
    CHECK(&toolkit.theme() == &default_theme());
    CHECK(same(toolkit.theme().colors.accent, rgb(0x22C7DC)));
    toolkit.draw_rounded_rect(nullptr, {0, 0, 5, 5}, CornerRadii::uniform(2.0f), kWhite);
    toolkit.draw_shadow(nullptr, {0, 0, 5, 5}, CornerRadii{}, ShadowSpec{});
    std::cout << "test_toolkit_extended_drawing passed\n";
}

// A toolkit written against the ORIGINAL four-method interface must still compile and work.
class LegacyToolkit final : public IWidgetToolkit {
public:
    struct Call {
        int32_t x, y;
        uint32_t w, h;
        Color c;
    };
    std::vector<Call> rects;
    void draw_rect(std::shared_ptr<ISurface>, int32_t x, int32_t y, uint32_t w, uint32_t h, Color c) override { rects.push_back({x, y, w, h, c}); }
    void draw_text(std::shared_ptr<ISurface>, int32_t, int32_t, std::string_view, Color) override {}
    void draw_button(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t, std::string_view) override {}
    void draw_panel(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t) override {}
};

void test_legacy_toolkit_fallbacks() {
    LegacyToolkit legacy;
    auto s = std::make_shared<SoftwareSurface>(10, 10);

    legacy.draw_rounded_rect(s, {3, 4, 20, 10}, CornerRadii::uniform(5.0f), Color{1, 2, 3, 255});
    CHECK(legacy.rects.size() == 1);
    CHECK(legacy.rects[0].x == 3 && legacy.rects[0].y == 4 && legacy.rects[0].w == 20 && legacy.rects[0].h == 10);

    legacy.rects.clear();
    legacy.draw_rounded_border(s, {0, 0, 20, 20}, CornerRadii::uniform(5.0f), 2.0f, kWhite);
    CHECK(legacy.rects.size() == 4);
    legacy.rects.clear();
    legacy.draw_rounded_border(s, {0, 0, 3, 3}, CornerRadii{}, 2.0f, kWhite); // too small for a hollow border
    CHECK(legacy.rects.size() == 1);

    legacy.rects.clear();
    legacy.draw_gradient_rect(s, {0, 0, 5, 5}, CornerRadii{}, LinearGradient{Color{9, 8, 7, 255}, Color{0, 0, 0, 255}, true, true});
    CHECK(legacy.rects.size() == 1 && same(legacy.rects[0].c, Color{9, 8, 7, 255}));

    legacy.rects.clear();
    legacy.draw_shadow(s, {0, 0, 5, 5}, CornerRadii{}, ShadowSpec{8, 0, 0, 0, Color{0, 0, 0, 100}});
    CHECK(legacy.rects.empty()); // shadows are optional decoration

    CHECK(&legacy.theme() == &default_theme());
    std::cout << "test_legacy_toolkit_fallbacks passed\n";
}

} // namespace

int main() {
#if defined(_MSC_VER)
    // A failed assertion must print and exit, never open a modal dialog that would hang an unattended run.
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    std::cout << "Running Painter Tests...\n";
    test_fill_rect_exact();
    test_fill_rect_bounds_and_degenerate();
    test_blend_onto_transparent_keeps_colour();
    test_blend_onto_opaque();
    test_blend_translucent_over_translucent();
    test_rounded_rect_corners_and_edges();
    test_rounded_rect_symmetry_and_area();
    test_rounded_rect_radius_clamp_and_zero();
    test_rounded_rect_per_corner();
    test_stroke_square();
    test_stroke_rounded();
    test_circle();
    test_stroke_circle();
    test_line();
    test_linear_gradient();
    test_gradient_to_transparent_has_no_grey_fringe();
    test_rounded_gradient();
    test_radial_gradient();
    test_clipping();
    test_coverage_mask();
    test_shadow();
    test_draw_box_square_is_exact();
    test_draw_box_has_no_gap_between_fill_and_border();
    test_draw_box_shares_the_fill_geometry();
    test_draw_box_rounded_ring_and_interior();
    test_draw_box_degenerate_arguments();
    test_draw_box_clipping();
    test_draw_box_translucent_colours();
    test_draw_box_fuzz();
    test_bounds_safety_fuzz();
    test_no_heap_allocation_while_drawing();
    test_toolkit_draw_rect_compatibility();
    test_toolkit_extended_drawing();
    test_legacy_toolkit_fallbacks();
    std::cout << "All painter tests passed successfully.\n";
    return 0;
}
