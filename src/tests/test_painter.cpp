// Pixel-level tests for the anti-aliased Painter and its integration with BasicWidgetToolkit.
//
// Covers: exact fills, straight-alpha blending (including onto transparent / translucent destinations),
// anti-aliased shapes (symmetry, area), strokes, lines, gradients, clipping, shadows, bounds safety under a
// randomised fuzz with guard bytes, and the guarantee that drawing performs no heap allocation.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>
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
    assert(get(s, 5, 5) == (Px{255, 0, 0, 255}));
    assert(get(s, 14, 14) == (Px{255, 0, 0, 255}));
    assert(alpha(s, 4, 5) == 0 && alpha(s, 15, 14) == 0 && alpha(s, 5, 4) == 0 && alpha(s, 14, 15) == 0);
    assert(count_alpha_nonzero(s) == 100);
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
        assert(count_alpha_nonzero(s) == 50);
        assert(alpha(s, 4, 4) == 255 && alpha(s, 5, 5) == 0 && alpha(s, 15, 15) == 255 && alpha(s, 14, 14) == 0);
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
        assert(count_alpha_nonzero(s) == 400);
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
        assert(count_alpha_nonzero(s) == 0);
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
    assert(get(s, 0, 0) == (Px{255, 0, 0, 128}));
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
    assert(p.r == 255 && near(p.g, 127, 1) && near(p.b, 127, 1) && p.a == 255);
    assert(get(s, 3, 3) == (Px{0, 0, 0, 255}));
    assert(get(s, 0, 3) == (Px{255, 255, 255, 255}));
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
    assert(a.r >= 254 && a.g >= 254 && a.b >= 254); // colour must stay white, not drift
    assert(near(a.a, 192, 1));                      // 128 + 128*(127/255)
    const Px b = get(s, 1, 0);
    assert(near(b.a, 192, 1));
    assert(near(b.r, 170, 2) && near(b.b, 85, 2) && b.g == 0); // newer (red) layer carries 2/3 of the weight
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
    assert(alpha(s, 4, 4) == 0 && alpha(s, 5, 5) == 0); // outside the arc
    const int partial = alpha(s, 6, 6);                 // straddles the arc: anti-aliased
    assert(partial > 100 && partial < 255);
    assert(alpha(s, 7, 7) == 255);                      // inside the arc
    assert(alpha(s, 20, 4) == 255 && alpha(s, 4, 20) == 255 && alpha(s, 35, 20) == 255 && alpha(s, 20, 35) == 255);
    assert(alpha(s, 20, 3) == 0 && alpha(s, 3, 20) == 0 && alpha(s, 36, 20) == 0 && alpha(s, 20, 36) == 0);
    assert(alpha(s, 20, 20) == 255);
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
            assert(a == alpha(s, 4 + 31 - x, 4 + y));
            assert(a == alpha(s, 4 + x, 4 + 31 - y));
        }
    }
    const double expected = 32.0 * 32.0 - (4.0 - kPi) * 8.0 * 8.0; // square minus four corner voids
    assert(near_rel(area(s), expected, 0.01));
    std::cout << "test_rounded_rect_symmetry_and_area passed\n";
}

void test_rounded_rect_radius_clamp_and_zero() {
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::uniform(1000.0f), kWhite); }
        assert(near_rel(area(s), kPi * 16.0 * 16.0, 0.02)); // clamped to half the side: a circle
    }
    {
        SoftwareSurface s(60, 24);
        { Painter p(s); p.fill_rounded_rect({4, 4, 40, 16}, CornerRadii::uniform(1000.0f), kWhite); }
        assert(near_rel(area(s), kPi * 8.0 * 8.0 + 24.0 * 16.0, 0.02)); // pill
    }
    {
        SoftwareSurface a(30, 30), b(30, 30);
        { Painter p(a); p.fill_rounded_rect({3, 3, 20, 20}, CornerRadii::uniform(0.0f), kRed); }
        { Painter p(b); p.fill_rect({3, 3, 20, 20}, kRed); }
        assert(area(a) == area(b));
        for (int y = 0; y < 30; ++y) for (int x = 0; x < 30; ++x) assert(get(a, x, y) == get(b, x, y));
    }
    {
        SoftwareSurface s(30, 30);
        {
            Painter p(s);
            p.fill_rounded_rect({3, 3, 20, 20}, CornerRadii::uniform(-5.0f), kRed);
            p.fill_rounded_rect({3, 3, 20, 20}, CornerRadii::uniform(std::nanf("")), kRed);
        }
        assert(count_alpha_nonzero(s) == 400); // negative / NaN radii behave as square
    }
    std::cout << "test_rounded_rect_radius_clamp_and_zero passed\n";
}

void test_rounded_rect_per_corner() {
    SoftwareSurface s(40, 40);
    {
        Painter p(s);
        p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::top(10.0f), kWhite);
    }
    assert(alpha(s, 4, 4) == 0 && alpha(s, 35, 4) == 0); // rounded top corners
    assert(alpha(s, 4, 35) == 255 && alpha(s, 35, 35) == 255); // square bottom corners
    std::cout << "test_rounded_rect_per_corner passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Strokes
// ---------------------------------------------------------------------------------------------------------
void test_stroke_square() {
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.stroke_rounded_rect({2, 2, 36, 36}, CornerRadii{}, 1.0f, kWhite); }
        assert(alpha(s, 2, 2) == 255 && alpha(s, 20, 2) == 255 && alpha(s, 2, 20) == 255 && alpha(s, 37, 37) == 255);
        assert(alpha(s, 3, 3) == 0 && alpha(s, 20, 20) == 0 && alpha(s, 20, 3) == 0);
        assert(alpha(s, 20, 1) == 0 && alpha(s, 1, 20) == 0);
        size_t full = 0;
        auto m = s.map_pixels();
        for (size_t i = 3; i < m.size(); i += 4) full += (m[i] == 255);
        s.unmap_pixels();
        assert(full == 4 * 36 - 4);
    }
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.stroke_rounded_rect({2, 2, 36, 36}, CornerRadii{}, 2.0f, kWhite); }
        assert(alpha(s, 3, 20) == 255 && alpha(s, 4, 20) == 0);
    }
    {
        SoftwareSurface s(40, 40);
        { Painter p(s); p.stroke_rounded_rect({2, 2, 36, 36}, CornerRadii{}, 500.0f, kWhite); }
        assert(count_alpha_nonzero(s) == 36 * 36); // thicker than the shape: solid
        assert(alpha(s, 20, 20) == 255);
    }
    std::cout << "test_stroke_square passed\n";
}

void test_stroke_rounded() {
    SoftwareSurface s(60, 60);
    { Painter p(s); p.stroke_rounded_rect({4, 4, 52, 52}, CornerRadii::uniform(8.0f), 1.5f, kWhite); }
    assert(alpha(s, 30, 30) == 0); // interior untouched
    assert(alpha(s, 4, 4) == 0);   // outside the rounded corner
    const double perimeter = 2.0 * ((52 - 16) + (52 - 16)) + 2.0 * kPi * 8.0;
    assert(near_rel(area(s), perimeter * 1.5, 0.06));
    // Mirror symmetry of the ring.
    for (int y = 0; y < 52; ++y) assert(alpha(s, 4 + y, 4) == alpha(s, 4 + y, 4 + 51) && alpha(s, 4, 4 + y) == alpha(s, 4 + 51, 4 + y));
    std::cout << "test_stroke_rounded passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Circles and lines
// ---------------------------------------------------------------------------------------------------------
void test_circle() {
    SoftwareSurface s(41, 41);
    { Painter p(s); p.fill_circle(20.5f, 20.5f, 10.0f, kWhite); }
    assert(alpha(s, 20, 20) == 255);
    assert(alpha(s, 20, 5) == 0 && alpha(s, 5, 20) == 0);
    assert(near_rel(area(s), kPi * 100.0, 0.01));
    for (int y = 0; y < 41; ++y) for (int x = 0; x < 41; ++x) assert(alpha(s, x, y) == alpha(s, 40 - x, y) && alpha(s, x, y) == alpha(s, x, 40 - y));

    // Centre on a pixel corner: the four surrounding pixels are identical.
    SoftwareSurface c(20, 20);
    { Painter p(c); p.fill_circle(10.0f, 10.0f, 4.0f, kWhite); }
    assert(alpha(c, 9, 9) == alpha(c, 10, 9) && alpha(c, 9, 9) == alpha(c, 9, 10) && alpha(c, 9, 9) == alpha(c, 10, 10));

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
    assert(count_alpha_nonzero(d) == 0);

    // A huge circle that covers the surface completes quickly and fills it.
    SoftwareSurface big(20, 20);
    { Painter p(big); p.fill_circle(10, 10, 1e7f, kWhite); }
    assert(count_alpha_nonzero(big) == 400);
    std::cout << "test_circle passed\n";
}

void test_stroke_circle() {
    SoftwareSurface s(41, 41);
    { Painter p(s); p.stroke_circle(20.5f, 20.5f, 10.0f, 2.0f, kWhite); }
    assert(alpha(s, 20, 20) == 0); // hole
    assert(near_rel(area(s), kPi * (100.0 - 64.0), 0.03));
    SoftwareSurface t(41, 41);
    { Painter p(t); p.stroke_circle(20.5f, 20.5f, 10.0f, 500.0f, kWhite); } // thicker than radius: solid disc
    assert(near_rel(area(t), kPi * 100.0, 0.01));
    std::cout << "test_stroke_circle passed\n";
}

void test_line() {
    {
        SoftwareSurface s(40, 20);
        { Painter p(s); p.draw_line(2.0f, 10.5f, 30.0f, 10.5f, 1.0f, kWhite); }
        for (int x = 3; x < 29; ++x) assert(alpha(s, x, 10) == 255);
        for (int x = 0; x < 40; ++x) assert(alpha(s, x, 9) == 0 && alpha(s, x, 11) == 0);
        assert(alpha(s, 1, 10) > 0 && alpha(s, 1, 10) < 255); // round cap, anti-aliased
        assert(alpha(s, 0, 10) == 0 && alpha(s, 31, 10) == 0);
    }
    {
        SoftwareSurface s(40, 20);
        { Painter p(s); p.draw_line(2.0f, 10.0f, 30.0f, 10.0f, 2.0f, kWhite); }
        assert(alpha(s, 15, 9) == 255 && alpha(s, 15, 10) == 255 && alpha(s, 15, 8) == 0 && alpha(s, 15, 11) == 0);
    }
    {
        SoftwareSurface s(30, 30);
        { Painter p(s); p.draw_line(2.0f, 2.0f, 17.0f, 17.0f, 1.0f, kWhite); }
        assert(alpha(s, 9, 9) == 255);
        for (int y = 0; y < 30; ++y) for (int x = 0; x < 30; ++x) assert(alpha(s, x, y) == alpha(s, y, x)); // symmetric under transpose
    }
    {
        SoftwareSurface s(30, 30);
        { Painter p(s); p.draw_line(15.0f, 15.0f, 15.0f, 15.0f, 4.0f, kWhite); } // zero length -> round dot
        assert(near_rel(area(s), kPi * 4.0, 0.06));
    }
    {
        SoftwareSurface s(40, 20); // a line that is enormous in both directions is clipped, not iterated
        { Painter p(s); p.draw_line(-1e6f, 10.5f, 1e6f, 10.5f, 1.0f, kWhite); }
        size_t full = 0;
        for (int x = 0; x < 40; ++x) full += (alpha(s, x, 10) == 255);
        assert(full == 40);
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
        assert(count_alpha_nonzero(s) == 0);
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
    assert(get(s, 3, 0).r >= 252);
    assert(get(s, 3, 63).r <= 3);
    assert(near(get(s, 3, 32).r, 127, 3));
    int prev = 256;
    for (int y = 0; y < 64; ++y) {
        const Px p = get(s, 3, y);
        assert(p.r == p.g && p.g == p.b && p.a == 255);
        assert(p.r <= prev);
        prev = p.r;
        for (int x = 1; x < 8; ++x) assert(get(s, x, y) == get(s, 0, y)); // no horizontal variation without dither
    }

    // Dithering is deterministic, stays within one level of the plain gradient, and varies across a row.
    const LinearGradient g_dither{Color{255, 255, 255, 255}, Color{0, 0, 0, 255}, true, true};
    SoftwareSurface d1(8, 64), d2(8, 64);
    { Painter p(d1); p.fill_gradient({0, 0, 8, 64}, g_dither); }
    { Painter p(d2); p.fill_gradient({0, 0, 8, 64}, g_dither); }
    bool varies = false;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 8; ++x) {
            assert(get(d1, x, y) == get(d2, x, y));
            assert(near(get(d1, x, y).r, get(s, x, y).r, 1));
            if (get(d1, x, y).r != get(d1, 0, y).r) varies = true;
        }
    }
    assert(varies);

    // Horizontal direction.
    const LinearGradient g_h{Color{0, 0, 0, 255}, Color{255, 255, 255, 255}, false, false};
    SoftwareSurface h(64, 4);
    { Painter p(h); p.fill_gradient({0, 0, 64, 4}, g_h); }
    assert(get(h, 0, 1).r <= 3 && get(h, 63, 1).r >= 252);
    assert(get(h, 10, 0) == get(h, 10, 3));

    // A constant colour is never disturbed by dithering.
    const LinearGradient flat{Color{40, 50, 60, 255}, Color{40, 50, 60, 255}, true, true};
    SoftwareSurface f(8, 8);
    { Painter p(f); p.fill_gradient({0, 0, 8, 8}, flat); }
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) assert(get(f, x, y) == (Px{40, 50, 60, 255}));
    std::cout << "test_linear_gradient passed\n";
}

void test_gradient_to_transparent_has_no_grey_fringe() {
    const LinearGradient g{Color{255, 255, 255, 255}, Color{255, 255, 255, 0}, true, false};
    SoftwareSurface s(4, 64);
    { Painter p(s); p.fill_gradient({0, 0, 4, 64}, g); }
    for (int y = 0; y < 64; ++y) {
        const Px p = get(s, 1, y);
        if (p.a > 0) assert(p.r >= 254 && p.g >= 254 && p.b >= 254); // colour is preserved while alpha fades
    }
    assert(near(get(s, 1, 32).a, 127, 4));
    assert(get(s, 1, 0).a >= 250 && get(s, 1, 63).a <= 4);
    std::cout << "test_gradient_to_transparent_has_no_grey_fringe passed\n";
}

void test_rounded_gradient() {
    const LinearGradient g{Color{200, 0, 0, 255}, Color{0, 0, 200, 255}, true, false};
    SoftwareSurface s(40, 40);
    { Painter p(s); p.fill_rounded_rect({4, 4, 32, 32}, CornerRadii::uniform(8.0f), g); }
    assert(alpha(s, 4, 4) == 0);
    assert(get(s, 20, 5).r > 180 && get(s, 20, 5).b < 20);
    assert(get(s, 20, 34).b > 180 && get(s, 20, 34).r < 20);
    std::cout << "test_rounded_gradient passed\n";
}

void test_radial_gradient() {
    SoftwareSurface s(41, 41);
    { Painter p(s); p.fill_radial_gradient(20.5f, 20.5f, 20.0f, Color{255, 255, 255, 255}, Color{255, 255, 255, 0}, false); }
    assert(alpha(s, 20, 20) >= 250);
    assert(alpha(s, 0, 0) == 0 && alpha(s, 40, 40) == 0); // beyond the radius: untouched
    assert(near(alpha(s, 30, 20), 134, 8));                // dist 9.5 -> ~52% of the way out
    int prev = 256;
    for (int x = 20; x < 41; ++x) {
        const int a = alpha(s, x, 20);
        assert(a <= prev);
        prev = a;
        if (a > 0) assert(get(s, x, 20).r >= 254);
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
}

void test_clipping() {
    SoftwareSurface s(30, 30);
    {
        Painter p(s);
        const Rect base = p.clip_bounds();
        assert(base.x == 0 && base.y == 0 && base.width == 30 && base.height == 30);
        assert(p.push_clip({10, 10, 10, 10}));
        draw_everything(p);
    }
    assert(count_alpha_nonzero_outside(s, 10, 10, 20, 20) == 0);
    assert(count_alpha_nonzero(s) > 50);

    // Nested clips intersect; popping restores the outer clip.
    SoftwareSurface n(30, 30);
    {
        Painter p(n);
        assert(p.push_clip({10, 10, 10, 10}));
        assert(p.push_clip({15, 15, 20, 20}));
        const Rect inner = p.clip_bounds();
        assert(inner.x == 15 && inner.y == 15 && inner.width == 5 && inner.height == 5);
        p.fill_rect({0, 0, 30, 30}, kWhite);
        p.pop_clip();
        const Rect outer = p.clip_bounds();
        assert(outer.x == 10 && outer.width == 10);
        p.fill_rect({0, 0, 30, 30}, kRed); // now fills the 10x10 clip (over the inner 5x5 white)
        p.pop_clip();
        p.pop_clip(); // popping the base clip is a harmless no-op
        assert(p.clip_bounds().width == 30);
    }
    assert(count_alpha_nonzero(n) == 100);
    assert(get(n, 17, 17) == (Px{255, 0, 0, 255}));

    // Empty clip: nothing is drawn.
    SoftwareSurface e(30, 30);
    {
        Painter p(e);
        assert(p.push_clip({100, 100, 5, 5})); // disjoint from the surface
        draw_everything(p);
    }
    assert(count_alpha_nonzero(e) == 0);

    // A clip that extends past the surface is trimmed to it.
    SoftwareSurface t(30, 30);
    {
        Painter p(t);
        assert(p.push_clip({-10, -10, 25, 25}));
        const Rect c = p.clip_bounds();
        assert(c.x == 0 && c.y == 0 && c.width == 15 && c.height == 15);
    }

    // Stack depth is bounded; ClipScope pops exactly what it pushed.
    SoftwareSurface d(30, 30);
    {
        Painter p(d);
        size_t pushed = 0;
        while (p.push_clip({0, 0, 30, 30})) ++pushed;
        assert(pushed == Painter::kMaxClipDepth - 1);
        {
            Painter::ClipScope scope(p, {0, 0, 5, 5});
            assert(!scope.pushed());
        }
        for (size_t i = 0; i < pushed; ++i) p.pop_clip();
        {
            Painter::ClipScope scope(p, {0, 0, 5, 5});
            assert(scope.pushed());
            p.fill_rect({0, 0, 30, 30}, kWhite);
        }
        assert(p.clip_bounds().width == 30);
    }
    assert(count_alpha_nonzero(d) == 25);
    std::cout << "test_clipping passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Shadows
// ---------------------------------------------------------------------------------------------------------
void test_shadow() {
    const ShadowSpec spec{12.0f, 0.0f, 6.0f, 0.0f, Color{0, 0, 0, 200}};
    const Rect box{30, 30, 60, 40};

    SoftwareSurface s(120, 100);
    { Painter p(s); p.draw_shadow(box, CornerRadii::uniform(8.0f), spec); }
    assert(alpha(s, 60, 50) == 0);  // cut out: the box interior is untouched
    assert(alpha(s, 0, 0) == 0);    // far away
    assert(alpha(s, 10, 50) == 0);  // beyond the blur band on the left
    int prev = 256;
    int peak = 0;
    for (int y = 70; y < 100; ++y) { // walking away from the box's lower edge
        const int a = alpha(s, 60, y);
        assert(a <= prev);
        assert(a <= 200);
        peak = std::max(peak, a);
        prev = a;
    }
    assert(peak > 120);              // visible just below the box
    assert(alpha(s, 60, 99) == 0);   // faded out by the bottom
    // Offset (0, +6) pushes the shadow down: more mass below the box than above it.
    double below = 0, above = 0;
    for (int x = 30; x < 90; ++x) {
        for (int y = 70; y < 100; ++y) below += alpha(s, x, y);
        for (int y = 0; y < 30; ++y) above += alpha(s, x, y);
    }
    assert(below > above * 1.5);

    // Without the cut-out, the interior carries the shadow too.
    SoftwareSurface filled(120, 100);
    { Painter p(filled); p.draw_shadow(box, CornerRadii::uniform(8.0f), spec, false); }
    assert(alpha(filled, 60, 50) >= 190);

    // With no offset the shadow is mirror-symmetric (within one level of float error).
    SoftwareSurface sym(120, 100);
    { Painter p(sym); p.draw_shadow(box, CornerRadii::uniform(8.0f), ShadowSpec{12.0f, 0, 0, 0, Color{0, 0, 0, 200}}); }
    for (int y = 0; y < 100; ++y) for (int x = 0; x < 120; ++x) assert(near(alpha(sym, x, y), alpha(sym, 119 - x, y), 1));

    // Hard shadow (blur 0) with an offset leaves an L-shaped sliver of exactly the peak alpha.
    SoftwareSurface hard(120, 100);
    { Painter p(hard); p.draw_shadow(box, CornerRadii{}, ShadowSpec{0.0f, 4, 4, 0, Color{0, 0, 0, 200}}); }
    assert(alpha(hard, 92, 50) == 200 && alpha(hard, 94, 50) == 0); // right sliver: x in [90,94)
    assert(alpha(hard, 60, 72) == 200 && alpha(hard, 60, 74) == 0); // bottom sliver
    assert(alpha(hard, 60, 50) == 0);

    // Spread grows the shape.
    SoftwareSurface spread(120, 100);
    { Painter p(spread); p.draw_shadow(box, CornerRadii{}, ShadowSpec{0.0f, 0, 0, 4, Color{0, 0, 0, 200}}); }
    assert(alpha(spread, 92, 50) == 200 && alpha(spread, 94, 50) == 0);

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
            assert(false);
        }
    }
}

void test_bounds_safety_fuzz() {
    const std::pair<uint32_t, uint32_t> sizes[] = {{37, 29}, {1, 1}, {1, 64}, {64, 1}, {128, 96}};
    uint64_t seed = 0x5EED;
    for (const auto& [w, h] : sizes) {
        for (int round = 0; round < 3; ++round) {
            GuardedSurface s(w, h);
            fuzz_painter(s, seed++, 1500);
            assert(s.guards_intact());
        }
    }

    // Surfaces the painter must refuse rather than trust.
    { GuardedSurface zero(0, 0); Painter p(zero); draw_everything(p); assert(zero.guards_intact()); }
    { GuardedSurface zero_w(0, 10); Painter p(zero_w); draw_everything(p); }
    {
        LyingSurface liar; // claims 100x100 but maps 16 bytes
        Painter p(liar);
        draw_everything(p);
        assert(p.width() == 0 && p.height() == 0);
        for (uint8_t b : liar.m_buf) assert(b == 0);
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
        assert(!probe->empty());
    }
    g_count_allocs = false;
    assert(g_alloc_count >= 2);

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
    }
    g_count_allocs = false;
    if (g_alloc_count != 0) std::cout << "  unexpected heap allocations: " << g_alloc_count.load() << "\n";
    assert(g_alloc_count == 0);
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
    assert(near(blended.r, static_cast<int>(10 * alpha_f + 200 * (1 - alpha_f)), 1));
    assert(near(blended.g, static_cast<int>(220 * alpha_f + 100 * (1 - alpha_f)), 1));
    assert(near(blended.b, static_cast<int>(90 * alpha_f + 50 * (1 - alpha_f)), 1));
    assert(blended.a == 255);
    assert(get(*surface, 15, 15) == (Px{200, 100, 50, 255}));

    // Onto a transparent surface (decorations): the colour is no longer darkened.
    auto deco = std::make_shared<SoftwareSurface>(8, 8);
    toolkit.draw_rect(deco, 0, 0, 4, 4, Color{200, 100, 50, 128});
    assert(get(*deco, 1, 1) == (Px{200, 100, 50, 128}));

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
    assert(alpha(*s, 4, 4) == 0 && alpha(*s, 30, 30) == 255);

    auto b = std::make_shared<SoftwareSurface>(60, 60);
    toolkit.draw_rounded_border(b, {4, 4, 52, 52}, CornerRadii::uniform(10.0f), 1.0f, kWhite);
    assert(alpha(*b, 30, 30) == 0 && alpha(*b, 30, 4) == 255);

    auto g = std::make_shared<SoftwareSurface>(60, 60);
    toolkit.draw_gradient_rect(g, {0, 0, 60, 60}, CornerRadii{}, LinearGradient{kWhite, Color{0, 0, 0, 255}, true, false});
    assert(get(*g, 5, 1).r > get(*g, 5, 58).r);

    auto sh = std::make_shared<SoftwareSurface>(100, 100);
    toolkit.draw_shadow(sh, {30, 30, 40, 40}, CornerRadii::uniform(6.0f), ShadowSpec{10, 0, 4, 0, Color{0, 0, 0, 180}});
    assert(alpha(*sh, 50, 50) == 0 && alpha(*sh, 50, 73) > 0);

    // The toolkit exposes the design tokens.
    assert(&toolkit.theme() == &default_theme());
    assert(same(toolkit.theme().colors.accent, rgb(0x22C7DC)));
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
    assert(legacy.rects.size() == 1);
    assert(legacy.rects[0].x == 3 && legacy.rects[0].y == 4 && legacy.rects[0].w == 20 && legacy.rects[0].h == 10);

    legacy.rects.clear();
    legacy.draw_rounded_border(s, {0, 0, 20, 20}, CornerRadii::uniform(5.0f), 2.0f, kWhite);
    assert(legacy.rects.size() == 4);
    legacy.rects.clear();
    legacy.draw_rounded_border(s, {0, 0, 3, 3}, CornerRadii{}, 2.0f, kWhite); // too small for a hollow border
    assert(legacy.rects.size() == 1);

    legacy.rects.clear();
    legacy.draw_gradient_rect(s, {0, 0, 5, 5}, CornerRadii{}, LinearGradient{Color{9, 8, 7, 255}, Color{0, 0, 0, 255}, true, true});
    assert(legacy.rects.size() == 1 && same(legacy.rects[0].c, Color{9, 8, 7, 255}));

    legacy.rects.clear();
    legacy.draw_shadow(s, {0, 0, 5, 5}, CornerRadii{}, ShadowSpec{8, 0, 0, 0, Color{0, 0, 0, 100}});
    assert(legacy.rects.empty()); // shadows are optional decoration

    assert(&legacy.theme() == &default_theme());
    std::cout << "test_legacy_toolkit_fallbacks passed\n";
}

} // namespace

int main() {
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
    test_shadow();
    test_bounds_safety_fuzz();
    test_no_heap_allocation_while_drawing();
    test_toolkit_draw_rect_compatibility();
    test_toolkit_extended_drawing();
    test_legacy_toolkit_fallbacks();
    std::cout << "All painter tests passed successfully.\n";
    return 0;
}
