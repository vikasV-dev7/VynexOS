// Tests for TextRenderer: measurement, drawing, readability, UTF-8/charset handling, clipping, bounds safety,
// determinism and the "no allocation after construction" guarantee.
#include <algorithm>
#include <array>
#include <atomic>
#include "test_check.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#include "../desktop/font_face.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/text_charset.hpp"
#include "vynexos/desktop/text_renderer.hpp"
#include "vynexos/desktop/theme.hpp"
#include "vynexos/desktop/utf8.hpp"

using namespace vynexos::desktop;

// ---------------------------------------------------------------------------------------------------------
// Allocation counter (same technique as test_painter): proves a region of code never touches the heap.
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

const TypographyTokens& tokens() { return default_theme().type; }

struct StyleCase {
    const char* name;
    TextStyle style;
};
std::vector<StyleCase> all_styles() {
    const auto& t = tokens();
    return {{"caption", t.caption}, {"body", t.body}, {"body_strong", t.body_strong}, {"title", t.title}, {"heading", t.heading}, {"display", t.display}};
}

constexpr Color kWhite{255, 255, 255, 255};

// A transparent canvas we draw text on and inspect.
struct Canvas {
    SoftwareSurface surface;
    explicit Canvas(uint32_t w, uint32_t h) : surface(w, h) {}

    std::vector<uint8_t> pixels() {
        auto m = surface.map_pixels();
        std::vector<uint8_t> v(m.begin(), m.end());
        surface.unmap_pixels();
        return v;
    }
    uint8_t alpha_at(int x, int y) {
        auto m = surface.map_pixels();
        const uint8_t a = m[(static_cast<size_t>(y) * surface.width() + static_cast<size_t>(x)) * 4 + 3];
        surface.unmap_pixels();
        return a;
    }
    struct Ink {
        bool any = false;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // inclusive bounds of pixels with alpha > 0
        unsigned long long sum = 0;
        size_t pixels = 0;
        size_t partial = 0;
    };
    Ink ink() {
        Ink r;
        auto m = surface.map_pixels();
        for (uint32_t y = 0; y < surface.height(); ++y) {
            for (uint32_t x = 0; x < surface.width(); ++x) {
                const uint8_t a = m[(static_cast<size_t>(y) * surface.width() + x) * 4 + 3];
                if (a == 0) continue;
                if (!r.any) {
                    r.any = true;
                    r.x0 = r.x1 = static_cast<int>(x);
                    r.y0 = r.y1 = static_cast<int>(y);
                }
                r.x0 = std::min(r.x0, static_cast<int>(x));
                r.x1 = std::max(r.x1, static_cast<int>(x));
                r.y0 = std::min(r.y0, static_cast<int>(y));
                r.y1 = std::max(r.y1, static_cast<int>(y));
                r.sum += a;
                ++r.pixels;
                if (a != 255) ++r.partial;
            }
        }
        surface.unmap_pixels();
        return r;
    }
};

std::unique_ptr<TextRenderer> make() {
    auto r = TextRenderer::create();
    CHECK(r != nullptr);
    return r;
}

float advance_px(FontWeight w, char32_t cp, float px) {
    const FontFace* f = embedded_face(w);
    return static_cast<float>(f->advance_units(f->glyph_index(cp))) * f->scale_for_em_pixels(px);
}

// ---------------------------------------------------------------------------------------------------------
void test_creation_and_options() {
    const auto t0 = std::chrono::steady_clock::now();
    auto r = make();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK(r->strike_count() == 6); // 12R, 14R, 14M, 15S, 20S, 32S
    CHECK(r->glyphs_per_strike() == supported_code_point_count() + 1);
    CHECK(r->atlas_bytes() > 20000 && r->atlas_bytes() < 2 * 1024 * 1024);
    CHECK(ms < 5000.0); // construction is a one-time startup cost; catches pathological slowness only
    std::cout << "  atlas: " << r->strike_count() << " strikes, " << r->glyphs_per_strike() << " glyphs each, " << r->atlas_bytes()
              << " bytes of coverage; built in " << ms << " ms\n";

    // Identical styles collapse into a single strike; unusable sizes are ignored; nothing usable -> null.
    TypographyTokens same = tokens();
    same.caption = same.body = same.body_strong = same.title = same.heading = same.display = tokens().body;
    CHECK(TextRenderer::create(same)->strike_count() == 1);
    TypographyTokens junk = tokens();
    junk.display.size_px = 100000.0f;
    junk.caption.size_px = 0.0f;
    CHECK(TextRenderer::create(junk)->strike_count() == 4); // body, body_strong, title, heading survive
    TypographyTokens none = tokens();
    none.caption.size_px = none.body.size_px = none.body_strong.size_px = none.title.size_px = none.heading.size_px = none.display.size_px = 0.0f;
    CHECK(TextRenderer::create(none) == nullptr);

    // The coverage curve adds weight when gamma > 1; non-positive gamma falls back to linear.
    TextRendererOptions heavy;
    heavy.coverage_gamma = 1.8f;
    TextRendererOptions bad;
    bad.coverage_gamma = -3.0f;
    auto ink_for = [](const TextRenderer& rr) {
        Canvas c(120, 40);
        Painter p(c.surface);
        rr.draw(p, 2, 26, "Hamburgefonstiv", tokens().body, kWhite);
        return c.ink().sum;
    };
    const auto base = ink_for(*r);
    CHECK(ink_for(*TextRenderer::create(tokens(), heavy)) > base);
    CHECK(ink_for(*TextRenderer::create(tokens(), bad)) == base);
    std::cout << "test_creation_and_options passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_measure_basics() {
    auto r = make();
    const TextStyle body = tokens().body;

    const TextMetrics empty = r->measure("", body);
    CHECK(empty.width == 0.0f && empty.glyphs == 0);
    CHECK(empty.line_height == body.line_height_px && empty.ascent > 0.0f && empty.descent > 0.0f);

    // Independent check against the font itself: one glyph's width is exactly its designed advance at this size.
    const float a = advance_px(FontWeight::Regular, U'A', body.size_px);
    CHECK(std::fabs(r->measure("A", body).width - a) < 1e-4f);
    const float b = advance_px(FontWeight::Regular, U'B', body.size_px);
    CHECK(std::fabs(r->measure("AB", body).width - (a + b)) < 1e-4f);
    CHECK(r->measure("AB", body).glyphs == 2);
    CHECK(r->measure(" ", body).width > 0.0f && r->measure(" ", body).glyphs == 1);

    // Additivity over a pile of deterministic pseudo-random ASCII strings.
    uint64_t s = 12345;
    auto next = [&] {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>(s >> 33);
    };
    for (int i = 0; i < 300; ++i) {
        std::string x, y;
        for (unsigned n = next() % 20; n > 0; --n) x.push_back(static_cast<char>(0x20 + next() % 95));
        for (unsigned n = next() % 20; n > 0; --n) y.push_back(static_cast<char>(0x20 + next() % 95));
        const float wx = r->measure(x, body).width;
        const float wy = r->measure(y, body).width;
        const float wxy = r->measure(x + y, body).width;
        CHECK(std::fabs(wxy - (wx + wy)) < 1e-3f);
    }

    // Wider with size, and the heavier weight is never narrower.
    const char* sample = "Hamburgefonstiv 0123456789";
    float prev = 0.0f;
    for (const TextStyle& st : {tokens().caption, tokens().body, tokens().title, tokens().heading, tokens().display}) {
        const float w = r->measure(sample, st).width;
        CHECK(w > prev);
        prev = w;
    }
    CHECK(r->measure(sample, tokens().body_strong).width >= r->measure(sample, tokens().body).width);

    // Line metrics come from the style / font and are sane for every token.
    for (const StyleCase& c : all_styles()) {
        const FontMetrics fm = r->font_metrics(c.style);
        CHECK(fm.ascent > 0 && fm.descent > 0);
        CHECK(fm.cap_height > fm.x_height && fm.x_height > 0);
        CHECK(fm.ascent >= fm.cap_height);
        CHECK(fm.line_height >= fm.ascent + fm.descent - 0.01f);
    }
    std::cout << "test_measure_basics passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_utf8_and_charset_handling() {
    auto r = make();
    const TextStyle body = tokens().body;
    auto w = [&](std::string_view s) { return r->measure(s, body); };

    // Supported multi-byte characters measure as their real glyphs.
    CHECK(std::fabs(w("\xC3\xA9").width - advance_px(FontWeight::Regular, 0xE9, 14.0f)) < 1e-4f);     // é
    CHECK(std::fabs(w("\xE2\x82\xAC").width - advance_px(FontWeight::Regular, 0x20AC, 14.0f)) < 1e-4f); // €
    CHECK(std::fabs(w("\xE2\x80\xA6").width - advance_px(FontWeight::Regular, 0x2026, 14.0f)) < 1e-4f); // …
    CHECK(std::fabs(w("\xE2\x86\x92").width - advance_px(FontWeight::Regular, 0x2192, 14.0f)) < 1e-4f); // →
    CHECK(w("Gr\xC3\xBC\xC3\x9F" "e").glyphs == 5);

    // Control characters: zero width, not counted (embedded NUL included).
    const std::string with_controls("a\tb\nc\r\0d\x01\x7F", 10);
    CHECK(w(with_controls).glyphs == 4 && std::fabs(w(with_controls).width - w("abcd").width) < 1e-4f);

    // Invisible format characters: zero width, not drawn, and not replaced by the box.
    for (const char* s : {"\xC2\xAD", "\xE2\x80\x8B", "\xE2\x80\x8C", "\xE2\x80\x8D", "\xE2\x81\xA0", "\xEF\xBB\xBF"}) {
        CHECK(w(s).width == 0.0f && w(s).glyphs == 0);
    }
    CHECK(std::fabs(w("a\xC2\xAD" "b").width - w("ab").width) < 1e-4f);

    // Unsupported scripts, emoji, combining marks and ill-formed bytes each become ONE replacement box.
    const float box = w("\xFF").width;
    CHECK(box > 0.0f && w("\xFF").glyphs == 1);
    for (const char* s : {"\xE4\xB8\xAD", "\xF0\x9F\x98\x80", "\xD0\x90", "\xCC\x81", "\xD7\x90", "\xE2\x82\xA8"}) {
        CHECK(w(s).glyphs == 1 && std::fabs(w(s).width - box) < 1e-4f); // CJK, emoji, Cyrillic, combining acute, Hebrew, rupee sign
    }
    // A combining mark is NOT silently merged onto the previous letter.
    CHECK(w("e\xCC\x81").glyphs == 2 && std::fabs(w("e\xCC\x81").width - (w("e").width + box)) < 1e-4f);
    // Truncated and overlong sequences: one box per ill-formed subpart, following ASCII survives.
    CHECK(w("\xE2\x82").glyphs == 1);                                    // truncated euro sign
    CHECK(w("a\xC0\xAF" "b").glyphs == 4);                               // a, box(C0), box(AF), b
    CHECK(w("\xED\xA0\x80").glyphs == 3);                                // encoded surrogate
    CHECK(w("\xF4\x90\x80\x80").glyphs == 4);                            // above U+10FFFF

    // Drawing agrees: every replacement looks identical, and differs from a real letter.
    auto bitmap = [&](std::string_view s) {
        Canvas c(60, 40);
        Painter p(c.surface);
        r->draw(p, 4, 28, s, body, kWhite);
        return c.pixels();
    };
    CHECK(bitmap("\xE4\xB8\xAD") == bitmap("\xFF") && bitmap("\xF0\x9F\x98\x80") == bitmap("\xFF"));
    CHECK(bitmap("\xFF") != bitmap("A"));
    CHECK(bitmap("\xC2\xAD") == bitmap("")); // invisible: draws nothing at all

    // Large input stays linear and exact.
    const std::string big(100000, 'a');
    const float one = advance_px(FontWeight::Regular, U'a', 14.0f);
    CHECK(std::fabs(w(big).width - one * 100000.0f) < one * 100000.0f * 1e-3f);
    std::cout << "test_utf8_and_charset_handling passed (replacement box advance " << box << " px)\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_draw_matches_measure() {
    auto r = make();
    const char* samples[] = {"Hamburgefonstiv", "The quick brown fox jumps over the lazy dog", "Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC 12,50 \xE2\x86\x92 \xE2\x80\xA2 \xE2\x80\xA6",
                             "iIl1| ,.;:'\"", "WWWW", "x"};
    for (const StyleCase& c : all_styles()) {
        for (const char* text : samples) {
            Canvas canvas(700, 80);
            const float x0 = 10.0f;
            const int baseline = 55;
            float drawn = 0.0f;
            {
                Painter p(canvas.surface);
                drawn = r->draw(p, x0, baseline, text, c.style, kWhite);
            }
            const TextMetrics m = r->measure(text, c.style);
            CHECK(drawn == m.width); // exactly equal: draw and measure share one accumulation

            const Canvas::Ink ink = canvas.ink();
            if (m.glyphs > 0 && std::string_view(text) != " ") CHECK(ink.any);
            if (!ink.any) continue;
            // Ink stays inside the measured pen travel, give or take side bearings and the anti-aliasing fringe.
            CHECK(ink.x0 >= static_cast<int>(x0) - 3);
            CHECK(ink.x1 <= static_cast<int>(x0 + m.width) + 3);
            CHECK(ink.y0 >= baseline - static_cast<int>(std::ceil(m.ascent)) - 2);
            CHECK(ink.y1 <= baseline + static_cast<int>(std::ceil(m.descent)) + 2);
        }
    }

    // Splitting a run is invisible: drawing "A" then "B" at the carried pen position equals drawing "AB".
    const TextStyle body = tokens().body;
    for (const char* text : {"Hello, World", "iiiiiiii", "AVAVAV", "m.m.m.m"}) {
        const std::string whole = text;
        const size_t cut = whole.size() / 2;
        Canvas a(300, 40), b(300, 40);
        {
            Painter p(a.surface);
            r->draw(p, 7.3f, 28, whole, body, kWhite);
        }
        {
            Painter p(b.surface);
            const float w1 = r->draw(p, 7.3f, 28, std::string_view(whole).substr(0, cut), body, kWhite);
            r->draw(p, 7.3f + w1, 28, std::string_view(whole).substr(cut), body, kWhite);
        }
        CHECK(a.pixels() == b.pixels());
    }

    // Fractional pen positions land on whole pixels and move ink at most one pixel.
    Canvas f0(200, 40), f1(200, 40);
    {
        Painter p0(f0.surface);
        r->draw(p0, 10.0f, 28, "Hello", body, kWhite);
        Painter p1(f1.surface);
        r->draw(p1, 10.4f, 28, "Hello", body, kWhite);
    }
    CHECK(std::abs(f0.ink().x0 - f1.ink().x0) <= 1);
    std::cout << "test_draw_matches_measure passed\n";
}

// ---------------------------------------------------------------------------------------------------------
Color composite_peak(const TextRenderer& r, std::string_view text, const TextStyle& style, Color fg, Color bg) {
    Canvas c(160, 50);
    {
        Painter p(c.surface);
        p.fill_rect({0, 0, 160, 50}, bg);
        r.draw(p, 6, 32, text, style, fg);
    }
    auto m = c.surface.map_pixels();
    Color best = bg;
    double best_contrast = 0.0;
    for (size_t i = 0; i < m.size(); i += 4) {
        const Color px{m[i], m[i + 1], m[i + 2], 255};
        const double cr = contrast_ratio(px, bg);
        if (cr > best_contrast) {
            best_contrast = cr;
            best = px;
        }
    }
    c.surface.unmap_pixels();
    return best;
}

void test_readability() {
    auto r = make();
    const auto& th = default_theme();

    // Real anti-aliasing: a healthy share of ink pixels are partial coverage, at every size.
    for (const StyleCase& c : all_styles()) {
        Canvas canvas(620, 90);
        {
            Painter p(canvas.surface);
            r->draw(p, 4, 60, "Hamburgefonstiv Hamburgefonstiv", c.style, kWhite);
        }
        const Canvas::Ink ink = canvas.ink();
        const double partial = static_cast<double>(ink.partial) / static_cast<double>(ink.pixels);
        std::cout << "  " << c.name << ": " << ink.pixels << " ink pixels, " << static_cast<int>(partial * 100) << "% partial coverage\n";
        // Small text is nearly all edge; large glyphs have solid interiors, so the expected share falls with size.
        CHECK(ink.pixels > 100 && partial > (c.style.size_px <= 15.0f ? 0.30 : 0.10));
    }

    // Proportions that make text legible: x-height and cap height at UI sizes.
    const FontMetrics body = r->font_metrics(tokens().body);
    const FontMetrics caption = r->font_metrics(tokens().caption);
    CHECK(body.x_height >= 7.0f && body.cap_height >= 9.5f);
    CHECK(caption.x_height >= 6.0f && caption.cap_height >= 8.5f);
    std::cout << "  body 14px: cap " << body.cap_height << " px, x-height " << body.x_height << " px; caption 12px: cap " << caption.cap_height
              << " px, x-height " << caption.x_height << " px\n";

    // Contrast of the BRIGHTEST pixel of thin strokes (an 'l' is one stem; anti-aliasing means it may never reach full
    // coverage) against the surface it sits on, for the token colours actually used.
    const Color bg = th.colors.surface;
    const Color primary = composite_peak(*r, "l", tokens().body, th.colors.text_primary, bg);
    const Color secondary = composite_peak(*r, "l", tokens().caption, th.colors.text_secondary, bg);
    const double cr_primary = contrast_ratio(primary, bg);
    const double cr_secondary = contrast_ratio(secondary, bg);
    std::cout << "  peak stem contrast: body/text_primary " << cr_primary << ":1, caption/text_secondary " << cr_secondary << ":1\n";
    CHECK(cr_primary >= 7.0);
    CHECK(cr_secondary >= 3.0); // WCAG minimum for non-text / large; the thinnest style on the dimmest text colour

    // Classic legibility trap: "rn" must not collapse into "m".
    auto render_to = [&](std::string_view t, const TextStyle& st) {
        Canvas c(40, 40);
        {
            Painter p(c.surface);
            r->draw(p, 4, 26, t, st, kWhite);
        }
        return c.pixels();
    };
    for (const TextStyle& st : {tokens().caption, tokens().body, tokens().title}) {
        const auto rn = render_to("rn", st);
        const auto m = render_to("m", st);
        unsigned long long diff = 0, ink_rn = 0, ink_m = 0;
        for (size_t i = 3; i < rn.size(); i += 4) {
            diff += static_cast<unsigned>(std::abs(int{rn[i]} - int{m[i]}));
            ink_rn += rn[i];
            ink_m += m[i];
        }
        const double rel = static_cast<double>(diff) / static_cast<double>(std::max(ink_rn, ink_m));
        CHECK(rel > 0.25);
    }

    // Neighbouring stems stay separate: "ll" always has an empty column between its two strokes.
    for (const StyleCase& c : all_styles()) {
        Canvas canvas(60, 60);
        {
            Painter p(canvas.surface);
            r->draw(p, 4, 44, "ll", c.style, kWhite);
        }
        const Canvas::Ink ink = canvas.ink();
        int gap_columns = 0;
        bool seen_ink = false;
        for (int x = ink.x0; x <= ink.x1; ++x) {
            bool col = false;
            for (int y = ink.y0; y <= ink.y1; ++y) col = col || canvas.alpha_at(x, y) > 40;
            if (col) {
                seen_ink = true;
            } else if (seen_ink) {
                ++gap_columns;
            }
        }
        CHECK(gap_columns >= 1);
    }

    // Medium carries visibly more ink than Regular at the same size (14px: body vs body_strong).
    auto ink_of = [&](const TextStyle& st) {
        Canvas c(300, 40);
        {
            Painter p(c.surface);
            r->draw(p, 4, 26, "Hamburgefonstiv", st, kWhite);
        }
        return c.ink().sum;
    };
    CHECK(ink_of(tokens().body_strong) > ink_of(tokens().body) * 1.04);
    std::cout << "test_readability passed\n";
}

// ---------------------------------------------------------------------------------------------------------
class GuardedSurface final : public ISurface {
public:
    static constexpr size_t kGuard = 256;
    GuardedSurface(uint32_t w, uint32_t h) : m_w(w), m_h(h), m_store(kGuard * 2 + static_cast<size_t>(w) * h * 4, 0xAB) {
        std::fill(m_store.begin() + kGuard, m_store.end() - kGuard, static_cast<uint8_t>(0));
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

void test_clipping_and_bounds() {
    auto r = make();
    const TextStyle body = tokens().body;
    const char* text = "The quick brown fox jumps over the lazy dog";

    // A clip rectangle confines every pixel of ink.
    {
        Canvas c(300, 60);
        {
            Painter p(c.surface);
            CHECK(p.push_clip({40, 15, 90, 25}));
            r->draw(p, 4, 38, text, body, kWhite);
        }
        const Canvas::Ink ink = c.ink();
        CHECK(ink.any);
        CHECK(ink.x0 >= 40 && ink.x1 < 130 && ink.y0 >= 15 && ink.y1 < 40);
    }
    // Empty clip draws nothing.
    {
        Canvas c(300, 60);
        {
            Painter p(c.surface);
            CHECK(p.push_clip({500, 500, 10, 10}));
            r->draw(p, 4, 38, text, body, kWhite);
        }
        CHECK(!c.ink().any);
    }
    // Text half outside the surface on every side is cropped, not corrupted.
    for (const auto& [x, baseline] : std::vector<std::pair<float, int32_t>>{{-80.0f, 30}, {250.0f, 30}, {10.0f, -2}, {10.0f, 70}, {10.0f, 5}, {-1000.0f, 30}}) {
        GuardedSurface s(120, 40);
        {
            Painter p(s);
            r->draw(p, x, baseline, text, body, kWhite);
        }
        CHECK(s.guards_intact());
    }

    // Randomised hostile input: arbitrary bytes, positions (incl. NaN/inf/extremes), styles and clips.
    uint64_t state = 0xC0FFEE;
    auto next = [&] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>(state >> 33);
    };
    auto wild_float = [&]() -> float {
        switch (next() % 10) {
        case 0: return std::numeric_limits<float>::quiet_NaN();
        case 1: return std::numeric_limits<float>::infinity();
        case 2: return -std::numeric_limits<float>::infinity();
        case 3: return 1e30f;
        case 4: return -1e30f;
        default: return static_cast<float>(static_cast<int>(next() % 600) - 300);
        }
    };
    const std::pair<uint32_t, uint32_t> sizes[] = {{97, 41}, {1, 1}, {200, 100}};
    for (const auto& [w, h] : sizes) {
        GuardedSurface s(w, h);
        Painter p(s);
        for (int i = 0; i < 1500; ++i) {
            std::string bytes(next() % 40, '\0');
            for (char& ch : bytes) ch = static_cast<char>(next() % 4 == 0 ? next() : 0x20 + next() % 95);
            TextStyle st;
            st.size_px = (next() % 4 == 0) ? wild_float() : tokens().body.size_px;
            st.weight = (next() & 1) ? FontWeight::Bold : FontWeight::Regular;
            const int32_t baseline = (next() % 6 == 0) ? static_cast<int32_t>(next()) : static_cast<int32_t>(next() % 120) - 20;
            switch (next() % 4) {
            case 0: r->draw(p, wild_float(), baseline, bytes, st, Color{static_cast<uint8_t>(next()), static_cast<uint8_t>(next()), static_cast<uint8_t>(next()), static_cast<uint8_t>(next())}); break;
            case 1: r->draw_ellipsized(p, wild_float(), baseline, bytes, st, kWhite, wild_float()); break;
            case 2: (void)r->fit_prefix(bytes, st, wild_float()); break;
            default: (void)r->measure(bytes, st); break;
            }
            if (next() % 25 == 0) p.push_clip({static_cast<int32_t>(next() % 100) - 20, static_cast<int32_t>(next() % 50) - 10, next() % 120, next() % 60});
            if (next() % 6 == 0) p.pop_clip();
            CHECK(s.guards_intact());
        }
    }
    std::cout << "test_clipping_and_bounds passed\n";
}

// ---------------------------------------------------------------------------------------------------------
bool is_boundary(std::string_view text, size_t pos) {
    // pos is a boundary iff it is 0, text.size(), or the start of a decoded unit when scanning from the beginning.
    size_t p = 0;
    while (p < text.size()) {
        if (p == pos) return true;
        p += utf8::decode_next(text.substr(p)).length;
    }
    return pos == text.size();
}

void test_fit_prefix_and_ellipsis() {
    auto r = make();
    const TextStyle body = tokens().body;
    const std::string texts[] = {"Hello, wonderful world", "Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC 100 \xE2\x86\x92 ok \xE2\x80\xA6", "a\tb  c\xC2\xAD" "d", "x\xFF" "y\xE4\xB8\xAD" "z", "WWWWWWWWWWWW", ""};

    for (const std::string& text : texts) {
        const float full = r->measure(text, body).width;
        for (float max = 0.0f; max <= full + 12.0f; max += 0.7f) {
            const size_t n = r->fit_prefix(text, body, max);
            CHECK(n <= text.size() && is_boundary(text, n));
            const float w = r->measure(std::string_view(text).substr(0, n), body).width;
            CHECK(w <= max + 1e-3f);
            if (n < text.size()) {
                // Maximal: the next unit would overflow (zero-width units would have been absorbed already).
                const size_t next_end = n + utf8::decode_next(std::string_view(text).substr(n)).length;
                CHECK(r->measure(std::string_view(text).substr(0, next_end), body).width > max + 1e-4f);
            } else {
                CHECK(w <= full + 1e-3f);
            }
        }
        CHECK(r->fit_prefix(text, body, full + 100.0f) == text.size());
        CHECK(r->fit_prefix(text, body, std::numeric_limits<float>::infinity()) == text.size());
        CHECK(r->fit_prefix(text, body, -1.0f) == 0 && r->fit_prefix(text, body, std::numeric_limits<float>::quiet_NaN()) == 0);
    }

    // draw_ellipsized: never wider than the box, exact when it fits, ellipsis only when it must.
    const std::string long_text = "Documents and Settings of the administrator account";
    const float ellipsis_w = r->measure("\xE2\x80\xA6", body).width;
    const float full = r->measure(long_text, body).width;
    for (float max = 0.0f; max <= full + 20.0f; max += 1.3f) {
        Canvas c(500, 40);
        float drawn;
        {
            Painter p(c.surface);
            drawn = r->draw_ellipsized(p, 4, 28, long_text, body, kWhite, max);
        }
        CHECK(drawn <= max + 1e-3f);
        const Canvas::Ink ink = c.ink();
        if (max < ellipsis_w - 1e-3f) {
            CHECK(drawn == 0.0f && !ink.any);
            continue;
        }
        CHECK(ink.any && ink.x1 <= static_cast<int>(4 + max) + 2); // ink stays inside the box (+AA fringe)
        if (full <= max + 1e-4f) {
            Canvas plain(500, 40);
            {
                Painter p(plain.surface);
                r->draw(p, 4, 28, long_text, body, kWhite);
            }
            CHECK(c.pixels() == plain.pixels() && std::fabs(drawn - full) < 1e-3f); // fits: no ellipsis at all
        } else {
            // Identical to drawing the kept prefix followed by the ellipsis glyph.
            size_t keep = r->fit_prefix(long_text, body, max - ellipsis_w);
            while (keep > 0 && long_text[keep - 1] == ' ') --keep;
            Canvas manual(500, 40);
            {
                Painter p(manual.surface);
                const float pw = r->draw(p, 4, 28, std::string_view(long_text).substr(0, keep), body, kWhite);
                r->draw(p, 4 + pw, 28, "\xE2\x80\xA6", body, kWhite);
                CHECK(std::fabs(drawn - (pw + ellipsis_w)) < 1e-3f);
            }
            CHECK(c.pixels() == manual.pixels());
        }
    }
    // Invalid widths and non-finite origins are harmless.
    {
        Canvas c(100, 40);
        Painter p(c.surface);
        CHECK(r->draw_ellipsized(p, 4, 28, long_text, body, kWhite, -5.0f) == 0.0f);
        CHECK(r->draw_ellipsized(p, 4, 28, long_text, body, kWhite, std::numeric_limits<float>::quiet_NaN()) == 0.0f);
        CHECK(r->draw_ellipsized(p, 4, 28, "short", body, kWhite, std::numeric_limits<float>::infinity()) > 0.0f);
        (void)r->draw_ellipsized(p, std::numeric_limits<float>::quiet_NaN(), 28, long_text, body, kWhite, 50.0f);
    }
    std::cout << "test_fit_prefix_and_ellipsis passed\n";
}

// ---------------------------------------------------------------------------------------------------------
std::vector<uint8_t> render_suite(const TextRenderer& r) {
    std::vector<uint8_t> all;
    for (const StyleCase& c : all_styles()) {
        for (const char* text : {"Hamburgefonstiv 0123", "Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC \xE2\x86\x92 \xE2\x80\xA6", "\xE4\xB8\xAD" "x"}) {
            Canvas canvas(420, 60);
            {
                Painter p(canvas.surface);
                r.draw(p, 3.6f, 40, text, c.style, Color{200, 220, 240, 255});
            }
            const auto px = canvas.pixels();
            all.insert(all.end(), px.begin(), px.end());
        }
    }
    return all;
}

void test_determinism_and_threads() {
    auto a = make();
    auto b = make();
    const auto first = render_suite(*a);
    CHECK(first == render_suite(*a)); // same instance, repeated
    CHECK(first == render_suite(*b)); // two separately built atlases are bit-identical

    // The renderer is immutable: concurrent drawing from several threads matches the single-threaded result.
    std::vector<std::vector<uint8_t>> results(4);
    std::vector<std::thread> pool;
    for (size_t i = 0; i < results.size(); ++i) {
        pool.emplace_back([&, i] { results[i] = render_suite(*a); });
    }
    for (auto& t : pool) t.join();
    for (const auto& res : results) CHECK(res == first);
    std::cout << "test_determinism_and_threads passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_no_allocation_after_construction() {
    auto r = make();
    Canvas canvas(500, 80);
    const std::string text = "Documents \xE2\x80\xA2 Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC 12,50 \xE2\x86\x92 \xE4\xB8\xAD \xFF end";
    {
        Painter warm(canvas.surface); // warm up anything lazily initialised by the runtime
        r->draw(warm, 4, 40, text, tokens().body, kWhite);
        (void)r->measure(text, tokens().body);
    }

    // Prove the instrumentation can see an allocation, otherwise "0" below means nothing.
    g_alloc_count = 0;
    g_count_allocs = true;
    { auto probe = std::make_unique<std::vector<int>>(8); CHECK(!probe->empty()); }
    g_count_allocs = false;
    CHECK(g_alloc_count >= 2);

    const std::vector<StyleCase> styles = all_styles(); // built before counting starts (it allocates)
    g_alloc_count = 0;
    g_count_allocs = true;
    float sink = 0.0f;
    for (int i = 0; i < 200; ++i) {
        Painter p(canvas.surface);
        Painter::ClipScope clip(p, {10, 10, 400, 60});
        for (const StyleCase& c : styles) {
            sink += r->draw(p, 6.0f + static_cast<float>(i % 7) * 0.31f, 30 + (i % 20), text, c.style, kWhite);
            sink += r->measure(text, c.style).width;
            sink += static_cast<float>(r->fit_prefix(text, c.style, 120.0f));
            sink += r->draw_ellipsized(p, 6.0f, 40, text, c.style, Color{200, 200, 200, 255}, 150.0f);
            sink += r->font_metrics(c.style).cap_height;
        }
    }
    g_count_allocs = false;
    if (g_alloc_count != 0) std::cout << "  unexpected heap allocations: " << g_alloc_count.load() << "\n";
    CHECK(g_alloc_count == 0);
    CHECK(sink > 0.0f);
    std::cout << "test_no_allocation_after_construction passed\n";
}

} // namespace

int main() {
#if defined(_MSC_VER)
    // Failed assertions must print and exit, never open a modal dialog that would hang an unattended run.
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    std::cout << "Running Text Renderer Tests...\n";
    test_creation_and_options();
    test_measure_basics();
    test_utf8_and_charset_handling();
    test_draw_matches_measure();
    test_readability();
    test_clipping_and_bounds();
    test_fit_prefix_and_ellipsis();
    test_determinism_and_threads();
    test_no_allocation_after_construction();
    std::cout << "All text renderer tests passed successfully.\n";
    return 0;
}
