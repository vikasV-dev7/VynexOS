// Tests for typography through the widget toolkit: the legacy draw_text() now uses the anti-aliased renderer while
// staying layout-compatible, the new baseline-based API, the 8x8 fallback, and compatibility with toolkits written
// against the original four-method interface.
#include <algorithm>
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
#include <thread>
#include <tuple>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/desktop/text_renderer.hpp"
#include "vynexos/desktop/theme.hpp"

using namespace vynexos::desktop;

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

constexpr Color kWhite{255, 255, 255, 255};

struct Ink {
    bool any = false;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    size_t pixels = 0;
    size_t partial = 0;
};

// Ink = pixels with non-zero alpha on a surface that started transparent.
Ink ink_of(SoftwareSurface& s) {
    Ink r;
    auto m = s.map_pixels();
    for (uint32_t y = 0; y < s.height(); ++y) {
        for (uint32_t x = 0; x < s.width(); ++x) {
            const uint8_t a = m[(static_cast<size_t>(y) * s.width() + x) * 4 + 3];
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
            ++r.pixels;
            if (a != 255) ++r.partial;
        }
    }
    s.unmap_pixels();
    return r;
}

std::vector<uint8_t> bytes_of(SoftwareSurface& s) {
    auto m = s.map_pixels();
    std::vector<uint8_t> v(m.begin(), m.end());
    s.unmap_pixels();
    return v;
}

// ---------------------------------------------------------------------------------------------------------
void test_legacy_draw_text_is_vector_and_layout_compatible() {
    BasicWidgetToolkit toolkit;
    assert(toolkit.has_vector_text());

    auto vec = std::make_shared<SoftwareSurface>(220, 40);
    auto bmp = std::make_shared<SoftwareSurface>(220, 40);
    toolkit.draw_text(vec, 10, 12, "Hello World", kWhite);          // y = top of the legacy 8 px cell
    toolkit.draw_text_bitmap(bmp, 10, 12, "Hello World", kWhite);   // the original renderer, same arguments
    const Ink v = ink_of(*vec);
    const Ink b = ink_of(*bmp);
    assert(v.any && b.any);

    // Genuinely anti-aliased now (the bitmap font is strictly 1-bit).
    assert(b.partial == 0);
    assert(static_cast<double>(v.partial) / static_cast<double>(v.pixels) > 0.30);

    // Layout compatibility: starts where the old text started and is vertically centred on the old cell.
    assert(v.x0 >= 10 && v.x0 <= 13);
    const double old_centre = (b.y0 + b.y1) / 2.0;
    const double new_centre = (v.y0 + v.y1) / 2.0;
    std::cout << "  legacy cell: 8x8 ink rows " << b.y0 << ".." << b.y1 << " (centre " << old_centre << "), vector ink rows " << v.y0 << ".." << v.y1
              << " (centre " << new_centre << ")\n";
    assert(std::fabs(new_centre - old_centre) <= 2.0);

    // A translucent colour now blends instead of overwriting (the bitmap path stamped the raw colour).
    auto over = std::make_shared<SoftwareSurface>(120, 30);
    toolkit.draw_rect(over, 0, 0, 120, 30, Color{0, 0, 0, 255});
    toolkit.draw_text(over, 4, 6, "Blend", Color{255, 255, 255, 128});
    auto m = over->map_pixels();
    int brightest = 0;
    for (size_t i = 0; i < m.size(); i += 4) brightest = std::max<int>(brightest, m[i]);
    over->unmap_pixels();
    assert(brightest > 40 && brightest <= 130); // at most ~50% white over black (+rounding)

    // Non-ASCII text now renders (the bitmap path silently dropped every byte >= 0x80).
    auto utf = std::make_shared<SoftwareSurface>(120, 30);
    auto utf_bitmap = std::make_shared<SoftwareSurface>(120, 30);
    toolkit.draw_text(utf, 4, 6, "\xC3\xA9\xE2\x82\xAC\xE2\x86\x92", kWhite); // é € →
    toolkit.draw_text_bitmap(utf_bitmap, 4, 6, "\xC3\xA9\xE2\x82\xAC\xE2\x86\x92", kWhite);
    assert(ink_of(*utf).pixels > 30);
    assert(ink_of(*utf_bitmap).any == false);
    std::cout << "test_legacy_draw_text_is_vector_and_layout_compatible passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_styled_api_and_alignment() {
    BasicWidgetToolkit toolkit;
    const TextStyle body = default_theme().type.body;
    const TextRenderer* renderer = TextRenderer::shared_default();
    assert(renderer != nullptr);

    // Forwards to the shared engine.
    const TextMetrics tm = toolkit.measure_text("Hamburgefonstiv", body);
    assert(tm.width == renderer->measure("Hamburgefonstiv", body).width && tm.glyphs == 15);
    const FontMetrics fm = toolkit.font_metrics(body);
    assert(fm.cap_height == renderer->font_metrics(body).cap_height);

    // y is the BASELINE: capitals rest on it and rise by the cap height.
    auto s = std::make_shared<SoftwareSurface>(80, 50);
    toolkit.draw_text_styled(s, 8, 30, "HE", body, kWhite);
    const Ink ink = ink_of(*s);
    assert(ink.y1 >= 29 && ink.y1 <= 30);                                        // bottom row at the baseline (+AA fringe)
    assert(std::fabs((30.0f - static_cast<float>(ink.y0)) - fm.cap_height) <= 1.5f); // top = baseline - cap height (+AA fringe)

    // The documented way to centre text in a box: baseline = top + (height + cap_height) / 2.
    const int box_top = 7;
    const int box_h = 36;
    auto c = std::make_shared<SoftwareSurface>(80, 60);
    const int baseline = box_top + static_cast<int>(std::lround((static_cast<float>(box_h) + fm.cap_height) * 0.5f));
    toolkit.draw_text_styled(c, 8, baseline, "HE", body, kWhite);
    const Ink centred = ink_of(*c);
    const int gap_above = centred.y0 - box_top;
    const int gap_below = (box_top + box_h - 1) - centred.y1;
    std::cout << "  centred in a " << box_h << " px box: " << gap_above << " px above, " << gap_below << " px below\n";
    assert(std::abs(gap_above - gap_below) <= 1);

    // Each token style draws and measures, and heavier/larger styles are wider.
    const auto& t = default_theme().type;
    float last = 0.0f;
    for (const TextStyle& st : {t.caption, t.body, t.title, t.heading, t.display}) {
        auto sf = std::make_shared<SoftwareSurface>(400, 80);
        toolkit.draw_text_styled(sf, 4, 50, "Settings", st, kWhite);
        assert(ink_of(*sf).any);
        const float w = toolkit.measure_text("Settings", st).width;
        assert(w > last);
        last = w;
    }

    // Fit and ellipsis go through the same engine.
    const std::string text = "A reasonably long window title that needs trimming";
    const size_t fit = toolkit.fit_text_prefix(text, body, 120.0f);
    assert(fit > 0 && fit < text.size() && toolkit.measure_text(text.substr(0, fit), body).width <= 120.0f + 1e-3f);
    auto e = std::make_shared<SoftwareSurface>(300, 40);
    const float drawn = toolkit.draw_text_ellipsized(e, 4, 26, text, body, kWhite, 120.0f);
    assert(drawn <= 120.0f + 1e-3f && drawn > 90.0f);
    assert(ink_of(*e).x1 <= 4 + 120 + 2);
    std::cout << "test_styled_api_and_alignment passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_button_label_stays_inside() {
    BasicWidgetToolkit toolkit;
    for (const auto& [w, h, label] : std::vector<std::tuple<uint32_t, uint32_t, const char*>>{{120, 40, "Click Me"}, {40, 30, "Up"}, {80, 30, "Vynex"}}) {
        auto s = std::make_shared<SoftwareSurface>(200, 80);
        toolkit.draw_button(s, 20, 20, w, h, label);
        auto m = s->map_pixels();
        int x0 = 1000, x1 = -1, y0 = 1000, y1 = -1;
        for (uint32_t y = 0; y < 80; ++y) {
            for (uint32_t x = 0; x < 200; ++x) {
                const size_t i = (static_cast<size_t>(y) * 200 + x) * 4;
                if (m[i] > 200 && m[i + 1] > 200 && m[i + 2] > 200 && m[i + 3] == 255) { // label pixels are near-white; the button is grey
                    // The 2 px bevel highlight along the top edge is also light; exclude that row band.
                    if (y < 22) continue;
                    x0 = std::min<int>(x0, static_cast<int>(x));
                    x1 = std::max<int>(x1, static_cast<int>(x));
                    y0 = std::min<int>(y0, static_cast<int>(y));
                    y1 = std::max<int>(y1, static_cast<int>(y));
                }
            }
        }
        s->unmap_pixels();
        std::cout << "  button " << w << "x" << h << " \"" << label << "\": label ink x " << x0 << ".." << x1 << ", y " << y0 << ".." << y1 << "\n";
        assert(x1 >= x0 && y1 >= y0);                                   // the label is there
        assert(x0 >= 20 && x1 < static_cast<int>(20 + w));              // horizontally inside the button
        assert(y0 >= 20 && y1 < static_cast<int>(20 + h));              // vertically inside the button
        const double centre = (y0 + y1) / 2.0;
        assert(std::fabs(centre - (20 + h / 2.0)) <= 3.0);              // and roughly centred vertically
    }
    std::cout << "test_button_label_stays_inside passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_fallback_without_font_engine() {
    BasicWidgetToolkit fallback(nullptr);
    BasicWidgetToolkit reference;
    assert(!fallback.has_vector_text());

    // draw_text IS the original bitmap renderer when there is no engine.
    auto a = std::make_shared<SoftwareSurface>(200, 30);
    auto b = std::make_shared<SoftwareSurface>(200, 30);
    fallback.draw_text(a, 6, 8, "Fallback text", kWhite);
    reference.draw_text_bitmap(b, 6, 8, "Fallback text", kWhite);
    assert(bytes_of(*a) == bytes_of(*b));
    assert(ink_of(*a).any && ink_of(*a).partial == 0);

    // Measurement and layout fall back to the fixed 8 px cell.
    const TextStyle body = default_theme().type.body;
    assert(fallback.measure_text("abc", body).width == 24.0f && fallback.measure_text("abc", body).glyphs == 3);
    assert(fallback.fit_text_prefix("abcdefghij", body, 30.0f) == 3);
    assert(fallback.fit_text_prefix("abc", body, 1000.0f) == 3 && fallback.fit_text_prefix("abc", body, -1.0f) == 0);

    // Baseline API: the cell top is baseline - 9.
    auto c = std::make_shared<SoftwareSurface>(200, 30);
    auto d = std::make_shared<SoftwareSurface>(200, 30);
    fallback.draw_text_styled(c, 6, 20, "Styled", body, kWhite);
    reference.draw_text_bitmap(d, 6, 20 - 9, "Styled", kWhite);
    assert(bytes_of(*c) == bytes_of(*d));

    // Ellipsis fallback: 80 px = 10 cells -> 7 characters + "...".
    auto e = std::make_shared<SoftwareSurface>(200, 30);
    auto f = std::make_shared<SoftwareSurface>(200, 30);
    const float w = fallback.draw_text_ellipsized(e, 6, 20, "Documents and settings", body, kWhite, 80.0f);
    reference.draw_text_bitmap(f, 6, 20 - 9, "Documen...", kWhite);
    assert(bytes_of(*e) == bytes_of(*f) && w == 80.0f);

    // draw_button uses draw_text, so it keeps working with the fallback too.
    auto g = std::make_shared<SoftwareSurface>(100, 50);
    fallback.draw_button(g, 5, 5, 80, 30, "OK");
    assert(ink_of(*g).any);
    std::cout << "test_fallback_without_font_engine passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// A toolkit written against the ORIGINAL four-method interface: every new typography call must work through the
// inherited defaults, without it knowing about fonts.
class LegacyToolkit final : public IWidgetToolkit {
public:
    struct Text {
        int32_t x, y;
        std::string text;
    };
    std::vector<Text> texts;
    void draw_rect(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t, Color) override {}
    void draw_text(std::shared_ptr<ISurface>, int32_t x, int32_t y, std::string_view t, Color) override { texts.push_back({x, y, std::string(t)}); }
    void draw_button(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t, std::string_view) override {}
    void draw_panel(std::shared_ptr<ISurface>, int32_t, int32_t, uint32_t, uint32_t) override {}
};

void test_legacy_toolkit_gets_working_defaults() {
    LegacyToolkit legacy;
    auto s = std::make_shared<SoftwareSurface>(10, 10);
    const TextStyle body = default_theme().type.body;

    legacy.draw_text_styled(s, 5, 30, "Hi", body, kWhite);
    assert(legacy.texts.size() == 1 && legacy.texts[0].x == 5 && legacy.texts[0].y == 21 && legacy.texts[0].text == "Hi");

    assert(legacy.measure_text("abcd", body).width == 32.0f);
    assert(legacy.measure_text("abcd", body).line_height == body.line_height_px);
    assert(legacy.font_metrics(body).cap_height > 0.0f && legacy.font_metrics(body).line_height == body.line_height_px);
    assert(legacy.fit_text_prefix("abcdefghij", body, 33.0f) == 4);
    assert(legacy.fit_text_prefix("abc", body, std::numeric_limits<float>::quiet_NaN()) == 0);

    legacy.texts.clear();
    const float fits = legacy.draw_text_ellipsized(s, 0, 20, "short", body, kWhite, 100.0f);
    assert(legacy.texts.size() == 1 && legacy.texts[0].text == "short" && fits == 40.0f);
    legacy.texts.clear();
    const float cut = legacy.draw_text_ellipsized(s, 0, 20, "a much longer string", body, kWhite, 80.0f);
    assert(legacy.texts.size() == 1 && legacy.texts[0].text == "a much ..." && cut == 80.0f);

    assert(&legacy.theme() == &default_theme());
    std::cout << "test_legacy_toolkit_gets_working_defaults passed\n";
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

void test_robustness_and_allocation() {
    BasicWidgetToolkit toolkit;
    const TextStyle body = default_theme().type.body;

    // Null surfaces are harmless on every text entry point.
    toolkit.draw_text(nullptr, 0, 0, "x", kWhite);
    toolkit.draw_text_styled(nullptr, 0, 0, "x", body, kWhite);
    assert(toolkit.draw_text_ellipsized(nullptr, 0, 0, "x", body, kWhite, 50.0f) == 0.0f);
    toolkit.draw_text_bitmap(nullptr, 0, 0, "x", kWhite);

    // Hostile arguments never write outside the surface.
    uint64_t state = 99;
    auto next = [&] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>(state >> 33);
    };
    GuardedSurface guarded(61, 23);
    auto shared_guarded = std::shared_ptr<ISurface>(&guarded, [](ISurface*) {});
    for (int i = 0; i < 2000; ++i) {
        std::string bytes(next() % 30, '\0');
        for (char& ch : bytes) ch = static_cast<char>(next() % 5 == 0 ? next() : 0x20 + next() % 95);
        const int32_t x = static_cast<int32_t>(next() % 200) - 100;
        const int32_t y = (next() % 8 == 0) ? static_cast<int32_t>(next()) : static_cast<int32_t>(next() % 100) - 40;
        switch (next() % 4) {
        case 0: toolkit.draw_text(shared_guarded, x, y, bytes, Color{255, 255, 255, static_cast<uint8_t>(next())}); break;
        case 1: toolkit.draw_text_styled(shared_guarded, x, y, bytes, body, kWhite); break;
        case 2: toolkit.draw_text_ellipsized(shared_guarded, x, y, bytes, body, kWhite, static_cast<float>(next() % 120) - 10.0f); break;
        default: toolkit.draw_button(shared_guarded, x, y, next() % 90, next() % 40, bytes); break;
        }
        assert(guarded.guards_intact());
    }

    // No heap allocation in any text call once the toolkit exists (the counter is proven to work first).
    auto s = std::make_shared<SoftwareSurface>(300, 60);
    toolkit.draw_text(s, 4, 6, "warm up", kWhite);
    g_alloc_count = 0;
    g_count_allocs = true;
    { auto probe = std::make_unique<std::vector<int>>(4); assert(!probe->empty()); }
    g_count_allocs = false;
    assert(g_alloc_count >= 2);

    const std::string text = "Documents \xE2\x80\xA2 Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC \xE4\xB8\xAD";
    float sink = 0.0f;
    g_alloc_count = 0;
    g_count_allocs = true;
    for (int i = 0; i < 300; ++i) {
        toolkit.draw_text(s, 4, 6 + (i % 5), text, kWhite);
        toolkit.draw_text_styled(s, 4, 30, text, body, kWhite);
        sink += toolkit.draw_text_ellipsized(s, 4, 40, text, body, kWhite, 130.0f);
        sink += toolkit.measure_text(text, body).width;
        sink += static_cast<float>(toolkit.fit_text_prefix(text, body, 90.0f));
        sink += toolkit.font_metrics(body).x_height;
        toolkit.draw_button(s, 150, 4, 100, 30, "Button");
    }
    g_count_allocs = false;
    if (g_alloc_count != 0) std::cout << "  unexpected heap allocations: " << g_alloc_count.load() << "\n";
    assert(g_alloc_count == 0);
    assert(sink > 0.0f);
    std::cout << "test_robustness_and_allocation passed\n";
}

// ---------------------------------------------------------------------------------------------------------
void test_shared_renderer_is_a_single_instance() {
    const TextRenderer* first = TextRenderer::shared_default();
    assert(first != nullptr && first == TextRenderer::shared_default());

    // First use from many threads at once must still agree on one instance (thread-safe static initialisation).
    std::vector<const TextRenderer*> seen(8, nullptr);
    std::vector<std::thread> pool;
    for (size_t i = 0; i < seen.size(); ++i) pool.emplace_back([&, i] { seen[i] = TextRenderer::shared_default(); });
    for (auto& t : pool) t.join();
    for (const TextRenderer* p : seen) assert(p == first);

    // Many toolkits share it rather than each building an atlas.
    BasicWidgetToolkit a;
    BasicWidgetToolkit b;
    const TextStyle body = default_theme().type.body;
    assert(a.measure_text("shared", body).width == b.measure_text("shared", body).width);
    std::cout << "test_shared_renderer_is_a_single_instance passed\n";
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
    std::cout << "Running Toolkit Text Tests...\n";
    test_shared_renderer_is_a_single_instance();
    test_legacy_draw_text_is_vector_and_layout_compatible();
    test_styled_api_and_alignment();
    test_button_label_stays_inside();
    test_fallback_without_font_engine();
    test_legacy_toolkit_gets_working_defaults();
    test_robustness_and_allocation();
    std::cout << "All toolkit text tests passed successfully.\n";
    return 0;
}
