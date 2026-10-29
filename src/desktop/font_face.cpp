// The only translation unit that sees stb_truetype. The library is vendored unmodified in third_party/stb and
// compiled here with its warnings silenced (it is third-party C-style code).
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wunused"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "font_face.hpp"
#include "font_data.hpp"
#include <array>
#include <cstring>

namespace vynexos::desktop {

struct FontFace::Impl {
    stbtt_fontinfo info{};
    int ascent{0};
    int descent{0};
    int line_gap{0};
    int units_per_em{1};
};

namespace {

[[nodiscard]] uint32_t be32(const unsigned char* p) noexcept {
    return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | uint32_t{p[3]};
}

[[nodiscard]] uint32_t tag(const char (&s)[5]) noexcept {
    return (uint32_t{static_cast<unsigned char>(s[0])} << 24) | (uint32_t{static_cast<unsigned char>(s[1])} << 16) |
           (uint32_t{static_cast<unsigned char>(s[2])} << 8) | uint32_t{static_cast<unsigned char>(s[3])};
}

} // namespace

FontFace::FontFace(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
FontFace::~FontFace() = default;
FontFace::FontFace(FontFace&&) noexcept = default;
FontFace& FontFace::operator=(FontFace&&) noexcept = default;

bool FontFace::validate(std::span<const unsigned char> data) noexcept {
    constexpr size_t kHeader = 12;
    constexpr size_t kRecord = 16;
    constexpr uint32_t kMaxTables = 64;
    if (data.size() < kHeader + kRecord || data.size() > (size_t{1} << 28)) return false;

    // 0x00010000 = TrueType outlines, 'true' = Apple TrueType, 'OTTO' = OpenType/CFF. ('ttcf' collections are not used.)
    const uint32_t version = be32(data.data());
    if (version != 0x00010000u && version != tag("true") && version != tag("OTTO")) return false;

    const uint32_t num_tables = (uint32_t{data[4]} << 8) | uint32_t{data[5]};
    if (num_tables == 0 || num_tables > kMaxTables) return false;
    if (kHeader + size_t{num_tables} * kRecord > data.size()) return false;

    struct Needed {
        uint32_t tag;
        uint32_t min_length;
        bool found;
    };
    std::array<Needed, 5> needed{{{tag("cmap"), 4, false}, {tag("head"), 54, false}, {tag("hhea"), 36, false},
                                  {tag("hmtx"), 4, false}, {tag("maxp"), 6, false}}};
    bool has_glyf = false;
    bool has_loca = false;
    bool has_cff = false;

    for (uint32_t i = 0; i < num_tables; ++i) {
        const unsigned char* rec = data.data() + kHeader + size_t{i} * kRecord;
        const uint32_t t = be32(rec);
        const uint64_t offset = be32(rec + 8);
        const uint64_t length = be32(rec + 12);
        if (offset > data.size() || length > data.size() - offset) return false; // table must lie inside the file
        for (Needed& n : needed) {
            if (n.tag == t && length >= n.min_length) n.found = true;
        }
        has_glyf = has_glyf || t == tag("glyf");
        has_loca = has_loca || t == tag("loca");
        has_cff = has_cff || t == tag("CFF ");
    }
    for (const Needed& n : needed) {
        if (!n.found) return false;
    }
    return (has_glyf && has_loca) || has_cff;
}

std::unique_ptr<FontFace> FontFace::create(std::span<const unsigned char> data) {
    if (!validate(data)) return nullptr;
    auto impl = std::make_unique<Impl>();
    if (stbtt_InitFont(&impl->info, data.data(), 0) == 0) return nullptr;
    stbtt_GetFontVMetrics(&impl->info, &impl->ascent, &impl->descent, &impl->line_gap);

    // units per em lives in the 'head' table at offset 18 (validated to exist and be >= 54 bytes).
    const uint32_t head_offset = impl->info.head;
    const unsigned char* head = data.data() + head_offset;
    impl->units_per_em = (int{head[18]} << 8) | int{head[19]};
    if (impl->units_per_em <= 0) return nullptr;

    return std::unique_ptr<FontFace>(new FontFace(std::move(impl)));
}

int FontFace::units_per_em() const noexcept { return m_impl->units_per_em; }
int FontFace::ascent_units() const noexcept { return m_impl->ascent; }
int FontFace::descent_units() const noexcept { return m_impl->descent; }
int FontFace::line_gap_units() const noexcept { return m_impl->line_gap; }

float FontFace::scale_for_em_pixels(float em_pixels) const noexcept {
    return em_pixels / static_cast<float>(m_impl->units_per_em);
}

int FontFace::glyph_index(char32_t code_point) const noexcept {
    if (code_point > 0x10FFFF) return 0;
    return stbtt_FindGlyphIndex(&m_impl->info, static_cast<int>(code_point));
}

int FontFace::advance_units(int glyph) const noexcept {
    int advance = 0;
    int lsb = 0;
    stbtt_GetGlyphHMetrics(&m_impl->info, glyph, &advance, &lsb);
    return advance;
}

FontFace::GlyphBox FontFace::glyph_box(int glyph, float scale) const noexcept {
    GlyphBox b{0, 0, 0, 0};
    stbtt_GetGlyphBitmapBox(&m_impl->info, glyph, scale, scale, &b.x0, &b.y0, &b.x1, &b.y1);
    return b;
}

void FontFace::rasterize(int glyph, float scale, unsigned char* out, int width, int height, int stride) const noexcept {
    if (out == nullptr || width <= 0 || height <= 0 || stride < width) return;
    stbtt_MakeGlyphBitmap(&m_impl->info, out, width, height, stride, scale, scale, glyph);
}

const FontFace* embedded_face(FontWeight weight) noexcept {
    // Function-local statics: built once on first use, thread-safe, never destroyed before use at shutdown.
    static const std::unique_ptr<FontFace> regular = FontFace::create(fonts::embedded_font_data(FontWeight::Regular));
    static const std::unique_ptr<FontFace> medium = FontFace::create(fonts::embedded_font_data(FontWeight::Medium));
    static const std::unique_ptr<FontFace> semibold = FontFace::create(fonts::embedded_font_data(FontWeight::SemiBold));
    switch (weight) {
    case FontWeight::Regular:
        return regular.get();
    case FontWeight::Medium:
        return medium.get();
    case FontWeight::SemiBold:
    case FontWeight::Bold:
    default:
        return semibold.get();
    }
}

} // namespace vynexos::desktop
