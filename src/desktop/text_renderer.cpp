#include "vynexos/desktop/text_renderer.hpp"
#include "font_face.hpp"
#include "vynexos/desktop/painter.hpp"
#include "vynexos/desktop/text_charset.hpp"
#include "vynexos/desktop/utf8.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vynexos::desktop {

namespace {

constexpr float kEpsilon = 1e-4f;

/// One rasterized glyph inside a strike. The bitmap is stored tightly (stride == w) in Strike::coverage.
struct Glyph {
    int16_t x0{0}; ///< bitmap left edge relative to the pen position
    int16_t y0{0}; ///< bitmap top edge relative to the baseline (negative = above)
    uint16_t w{0};
    uint16_t h{0};
    uint32_t offset{0};
    float advance{0.0f};
};

/// All glyphs for one size/weight. `glyphs` is indexed by "slot": the position of a code point within
/// kSupportedRanges, followed by one extra slot for the replacement (.notdef) glyph.
struct Strike {
    float px{0.0f};
    FontWeight weight{FontWeight::Regular};
    FontMetrics metrics{};
    std::vector<Glyph> glyphs;
    std::vector<uint8_t> coverage;
};

constexpr size_t kReplacementSlot = supported_code_point_count();

/// Slot of a supported code point, or -1.
[[nodiscard]] int slot_for(char32_t cp) noexcept {
    if (cp >= 0x20 && cp <= 0x7E) return static_cast<int>(cp - 0x20); // ASCII fast path (first range)
    int base = 0;
    for (const CodePointRange& r : kSupportedRanges) {
        if (cp >= r.first && cp <= r.last) return base + static_cast<int>(cp - r.first);
        base += static_cast<int>(r.last - r.first) + 1;
    }
    return -1;
}

/// Glyph to draw for a decoded code point; nullptr for characters that are skipped (controls, invisible format marks).
[[nodiscard]] const Glyph* resolve(const Strike& s, char32_t cp) noexcept {
    if (is_control_code_point(cp) || is_ignorable_code_point(cp)) return nullptr;
    const int slot = slot_for(cp);
    return &s.glyphs[slot >= 0 ? static_cast<size_t>(slot) : kReplacementSlot];
}

[[nodiscard]] FontWeight effective_weight(FontWeight w) noexcept {
    // Regular, Medium and SemiBold are embedded; anything heavier uses the heaviest available.
    return w == FontWeight::Bold ? FontWeight::SemiBold : w;
}

/// Calls fn(glyph, byte_offset_before, byte_offset_after) for every drawable glyph of `text`.
template <class Fn>
void for_each_glyph(const Strike& s, std::string_view text, Fn&& fn) noexcept {
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t start = pos;
        const utf8::Decoded d = utf8::decode_next(text.substr(pos));
        pos += d.length;
        const Glyph* g = resolve(s, d.valid ? d.code_point : utf8::kReplacement);
        if (g != nullptr && !fn(*g, start, pos)) return;
    }
}

} // namespace

struct TextRenderer::Impl {
    std::vector<Strike> strikes;
    size_t atlas_bytes{0};

    [[nodiscard]] const Strike& find(const TextStyle& style) const noexcept {
        const FontWeight want = effective_weight(style.weight);
        const Strike* best = &strikes.front();
        float best_cost = 1e30f;
        for (const Strike& s : strikes) {
            // Nearest size first (dominates), then nearest weight.
            const float cost = std::fabs(s.px - style.size_px) * 100.0f +
                               std::fabs(static_cast<float>(static_cast<int>(s.weight)) - static_cast<float>(static_cast<int>(want))) / 100.0f;
            if (cost < best_cost - 1e-6f) {
                best_cost = cost;
                best = &s;
            }
        }
        return *best;
    }

    /// Draws one glyph with the pen at `pen_x` (fractional); the glyph origin snaps to a whole pixel.
    static void draw_glyph(Painter& painter, const Strike& s, const Glyph& g, double pen_x, int32_t baseline_y, Color color) noexcept {
        if (g.w == 0 || g.h == 0) return;
        if (!(std::fabs(pen_x) < 1.0e8)) return; // also rejects NaN/inf
        const int32_t gx = static_cast<int32_t>(std::floor(pen_x + 0.5)) + g.x0;
        painter.draw_coverage_mask(gx, baseline_y + g.y0, g.w, g.h, s.coverage.data() + g.offset, g.w, color);
    }
};

TextRenderer::TextRenderer(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
TextRenderer::~TextRenderer() = default;

std::unique_ptr<TextRenderer> TextRenderer::create(const TypographyTokens& tokens, const TextRendererOptions& options) {
    // The distinct (size, weight) pairs the UI uses.
    struct Key {
        float px;
        FontWeight weight;
    };
    std::vector<Key> keys;
    for (const TextStyle* style : {&tokens.caption, &tokens.body, &tokens.body_strong, &tokens.title, &tokens.heading, &tokens.display}) {
        const Key k{style->size_px, effective_weight(style->weight)};
        const bool known = std::any_of(keys.begin(), keys.end(), [&](const Key& o) {
            return std::fabs(o.px - k.px) < 0.01f && o.weight == k.weight;
        });
        if (!known && k.px >= 4.0f && k.px <= 256.0f) keys.push_back(k);
    }
    if (keys.empty()) return nullptr;

    // Optional coverage curve, applied once here so drawing stays a straight blit.
    std::array<uint8_t, 256> curve{};
    const float gamma = options.coverage_gamma > 0.05f ? options.coverage_gamma : 1.0f;
    for (size_t i = 0; i < curve.size(); ++i) {
        const float c = static_cast<float>(i) / 255.0f;
        curve[i] = static_cast<uint8_t>(std::clamp(std::pow(c, 1.0f / gamma) * 255.0f + 0.5f, 0.0f, 255.0f));
    }
    const bool identity_curve = std::fabs(gamma - 1.0f) < 1e-6f;

    auto impl = std::make_unique<Impl>();
    impl->strikes.reserve(keys.size());

    for (const Key& key : keys) {
        const FontFace* face = embedded_face(key.weight);
        if (face == nullptr) return nullptr;
        const float scale = face->scale_for_em_pixels(key.px);

        Strike strike;
        strike.px = key.px;
        strike.weight = key.weight;
        strike.metrics.ascent = static_cast<float>(face->ascent_units()) * scale;
        strike.metrics.descent = -static_cast<float>(face->descent_units()) * scale;
        strike.metrics.line_height = strike.metrics.ascent + strike.metrics.descent;
        strike.metrics.cap_height = static_cast<float>(face->glyph_top_units(face->glyph_index(U'H'))) * scale;
        strike.metrics.x_height = static_cast<float>(face->glyph_top_units(face->glyph_index(U'x'))) * scale;

        // Slot -> glyph index: every supported code point in range order, then .notdef (glyph 0) as the replacement.
        std::vector<int> glyph_ids;
        glyph_ids.reserve(kReplacementSlot + 1);
        for (const CodePointRange& r : kSupportedRanges) {
            for (char32_t cp = r.first; cp <= r.last; ++cp) glyph_ids.push_back(face->glyph_index(cp));
        }
        glyph_ids.push_back(0);

        // Size the coverage buffer exactly, then rasterize straight into it.
        strike.glyphs.resize(glyph_ids.size());
        size_t total = 0;
        for (size_t i = 0; i < glyph_ids.size(); ++i) {
            const FontFace::GlyphBox box = face->glyph_box(glyph_ids[i], scale);
            Glyph& g = strike.glyphs[i];
            g.x0 = static_cast<int16_t>(box.x0);
            g.y0 = static_cast<int16_t>(box.y0);
            g.w = static_cast<uint16_t>(box.width());
            g.h = static_cast<uint16_t>(box.height());
            g.advance = static_cast<float>(face->advance_units(glyph_ids[i])) * scale;
            g.offset = static_cast<uint32_t>(total);
            total += static_cast<size_t>(g.w) * g.h;
        }
        strike.coverage.assign(total, 0);
        for (size_t i = 0; i < glyph_ids.size(); ++i) {
            const Glyph& g = strike.glyphs[i];
            if (g.w == 0 || g.h == 0) continue;
            face->rasterize(glyph_ids[i], scale, strike.coverage.data() + g.offset, g.w, g.h, g.w);
        }
        if (!identity_curve) {
            for (uint8_t& c : strike.coverage) c = curve[c];
        }
        impl->atlas_bytes += strike.coverage.size();
        impl->strikes.push_back(std::move(strike));
    }
    return std::unique_ptr<TextRenderer>(new TextRenderer(std::move(impl)));
}

TextMetrics TextRenderer::measure(std::string_view utf8, const TextStyle& style) const noexcept {
    const Strike& s = m_impl->find(style);
    TextMetrics m;
    m.ascent = s.metrics.ascent;
    m.descent = s.metrics.descent;
    m.line_height = style.line_height_px > 0.0f ? style.line_height_px : s.metrics.line_height;
    // Accumulate in double: summing very many float advances would drift by a visible fraction of a pixel per few
    // thousand characters. draw() and fit_prefix() use the identical accumulation, so they always agree with this.
    double width = 0.0;
    for_each_glyph(s, utf8, [&](const Glyph& g, size_t, size_t) {
        width += g.advance;
        ++m.glyphs;
        return true;
    });
    m.width = static_cast<float>(width);
    return m;
}

FontMetrics TextRenderer::font_metrics(const TextStyle& style) const noexcept {
    FontMetrics fm = m_impl->find(style).metrics;
    if (style.line_height_px > 0.0f) fm.line_height = style.line_height_px;
    return fm;
}

float TextRenderer::draw(Painter& painter, float x, int32_t baseline_y, std::string_view utf8, const TextStyle& style, Color color) const noexcept {
    const Strike& s = m_impl->find(style);
    double advance = 0.0; // accumulated from zero, exactly as measure() does, so the two always agree
    const bool placeable = std::isfinite(x);
    for_each_glyph(s, utf8, [&](const Glyph& g, size_t, size_t) {
        if (placeable) Impl::draw_glyph(painter, s, g, static_cast<double>(x) + advance, baseline_y, color);
        advance += g.advance;
        return true;
    });
    return static_cast<float>(advance);
}

size_t TextRenderer::fit_prefix(std::string_view utf8, const TextStyle& style, float max_width) const noexcept {
    if (!(max_width >= 0.0f)) return 0; // negative or NaN
    const Strike& s = m_impl->find(style);
    double width = 0.0;
    size_t fit = 0;
    for_each_glyph(s, utf8, [&](const Glyph& g, size_t start, size_t end) {
        if (width + g.advance > static_cast<double>(max_width) + kEpsilon) return false;
        width += g.advance;
        fit = end;
        (void)start;
        return true;
    });
    // Invisible characters directly after the last glyph that fit stay with the prefix (they have no width).
    while (fit < utf8.size()) {
        const utf8::Decoded d = utf8::decode_next(utf8.substr(fit));
        const char32_t cp = d.valid ? d.code_point : utf8::kReplacement;
        if (!(is_control_code_point(cp) || is_ignorable_code_point(cp))) break;
        fit += d.length;
    }
    return fit;
}

float TextRenderer::draw_ellipsized(Painter& painter, float x, int32_t baseline_y, std::string_view utf8, const TextStyle& style, Color color,
                                    float max_width) const noexcept {
    if (!(max_width >= 0.0f)) return 0.0f;
    const Strike& s = m_impl->find(style);
    const float full = measure(utf8, style).width;
    if (full <= max_width + kEpsilon) return draw(painter, x, baseline_y, utf8, style, color);

    const Glyph& ellipsis = s.glyphs[static_cast<size_t>(slot_for(0x2026))];
    if (ellipsis.advance > max_width + kEpsilon) return 0.0f; // not even the ellipsis fits

    size_t keep = fit_prefix(utf8, style, max_width - ellipsis.advance);
    while (keep > 0 && utf8[keep - 1] == ' ') --keep; // no dangling space before the ellipsis
    const float prefix_width = draw(painter, x, baseline_y, utf8.substr(0, keep), style, color);
    if (std::isfinite(x)) Impl::draw_glyph(painter, s, ellipsis, static_cast<double>(x) + prefix_width, baseline_y, color);
    return prefix_width + ellipsis.advance;
}

size_t TextRenderer::strike_count() const noexcept { return m_impl->strikes.size(); }
size_t TextRenderer::atlas_bytes() const noexcept { return m_impl->atlas_bytes; }
size_t TextRenderer::glyphs_per_strike() const noexcept { return kReplacementSlot + 1; }

} // namespace vynexos::desktop
