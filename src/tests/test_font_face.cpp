// Tests for the embedded Inter fonts and the FontFace wrapper.
//
// Usage: test_font_face <directory containing the Inter-*.ttf asset files>
// (CTest passes assets/fonts/inter; the directory is an argument rather than a compile definition so that paths with
// spaces need no escaping.)
#include <algorithm>
#include <array>
#include "test_check.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#include <cstdlib>
#endif
#include "../desktop/font_data.hpp"
#include "../desktop/font_face.hpp"
#include "vynexos/desktop/text_charset.hpp"

using namespace vynexos::desktop;

namespace {

std::string g_font_dir;

std::vector<unsigned char> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    CHECK(in.good());
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct Rng {
    uint64_t s;
    unsigned next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>(s >> 33);
    }
};

// ------------------------------------------------------------------------------------------------------------
void test_embedded_bytes_match_audited_assets() {
    struct Case {
        const char* file;
        FontWeight weight;
        size_t expected_size;
    };
    const Case cases[] = {{"Inter-Regular.ttf", FontWeight::Regular, 411640},
                          {"Inter-Medium.ttf", FontWeight::Medium, 417300},
                          {"Inter-SemiBold.ttf", FontWeight::SemiBold, 419744}};
    for (const Case& c : cases) {
        const auto file = read_file(g_font_dir + "/" + c.file);
        const auto embedded = fonts::embedded_font_data(c.weight);
        CHECK(file.size() == c.expected_size);
        CHECK(embedded.size() == c.expected_size);
        // bin2c must reproduce the audited file exactly: not a byte may differ.
        CHECK(std::memcmp(file.data(), embedded.data(), file.size()) == 0);
        // sfnt version 0x00010000: TrueType outlines.
        CHECK(embedded[0] == 0x00 && embedded[1] == 0x01 && embedded[2] == 0x00 && embedded[3] == 0x00);
    }
    // Bold has no embedded file of its own and maps to SemiBold.
    CHECK(fonts::embedded_font_data(FontWeight::Bold).data() == fonts::embedded_font_data(FontWeight::SemiBold).data());
    std::cout << "test_embedded_bytes_match_audited_assets passed\n";
}

// ------------------------------------------------------------------------------------------------------------
void test_validate_accepts_real_fonts_and_rejects_garbage() {
    for (FontWeight w : {FontWeight::Regular, FontWeight::Medium, FontWeight::SemiBold}) {
        CHECK(FontFace::validate(fonts::embedded_font_data(w)));
    }

    // Nothing here is handed to stb: validate()/create() must reject these before the parser sees them.
    CHECK(!FontFace::validate({}));
    CHECK(FontFace::create({}) == nullptr);

    Rng rng{0xF0F7};
    for (int i = 0; i < 2000; ++i) {
        std::vector<unsigned char> junk(rng.next() % 2048);
        for (auto& b : junk) b = static_cast<unsigned char>(rng.next());
        CHECK(!FontFace::validate(junk));
        CHECK(FontFace::create(junk) == nullptr);
    }

    const auto real = fonts::embedded_font_data(FontWeight::Regular);
    // Truncations: the table directory points past the end of the file.
    for (size_t n : {size_t{4}, size_t{12}, size_t{28}, size_t{100}, size_t{4096}}) {
        CHECK(!FontFace::validate(real.first(n)));
    }
    // Exact boundary: the end of the last table. Cutting there is fine (the rest is only padding); one byte less
    // would amputate a table and must be rejected.
    {
        const unsigned num = (unsigned{real[4]} << 8) | real[5];
        size_t end = 0;
        for (unsigned i = 0; i < num; ++i) {
            const unsigned char* rec = real.data() + 12 + i * 16;
            const size_t offset = (size_t{rec[8]} << 24) | (size_t{rec[9]} << 16) | (size_t{rec[10]} << 8) | rec[11];
            const size_t length = (size_t{rec[12]} << 24) | (size_t{rec[13]} << 16) | (size_t{rec[14]} << 8) | rec[15];
            end = std::max(end, offset + length);
        }
        CHECK(end <= real.size() && end > 4096);
        CHECK(FontFace::validate(real.first(end)));
        CHECK(!FontFace::validate(real.first(end - 1)));
    }

    auto patched = [&](auto&& mutate) {
        std::vector<unsigned char> copy(real.begin(), real.end());
        mutate(copy);
        return copy;
    };
    CHECK(!FontFace::validate(patched([](auto& d) { d[0] = 0x7F; })));                          // bad sfnt version
    CHECK(!FontFace::validate(patched([](auto& d) { d[0] = 't'; d[1] = 't'; d[2] = 'c'; d[3] = 'f'; }))); // collection
    CHECK(!FontFace::validate(patched([](auto& d) { d[4] = 0; d[5] = 0; })));                   // zero tables
    CHECK(!FontFace::validate(patched([](auto& d) { d[4] = 0xFF; d[5] = 0xFF; })));             // absurd table count
    // First table record: offset (bytes 20..23) pointing far outside the file.
    CHECK(!FontFace::validate(patched([](auto& d) { d[20] = 0xFF; d[21] = 0xFF; d[22] = 0xFF; d[23] = 0xF0; })));
    // First table record: length (bytes 24..27) overflowing the file.
    CHECK(!FontFace::validate(patched([](auto& d) { d[24] = 0xFF; d[25] = 0xFF; d[26] = 0xFF; d[27] = 0xFF; })));
    // Rename every 'cmap' table so a required table is missing.
    CHECK(!FontFace::validate(patched([](auto& d) {
        const unsigned num = (unsigned{d[4]} << 8) | d[5];
        for (unsigned i = 0; i < num; ++i) {
            unsigned char* rec = d.data() + 12 + i * 16;
            if (std::memcmp(rec, "cmap", 4) == 0) rec[0] = 'x';
        }
    })));
    std::cout << "test_validate_accepts_real_fonts_and_rejects_garbage passed\n";
}

// ------------------------------------------------------------------------------------------------------------
void test_faces_load_with_sane_metrics() {
    int em = 0;
    for (FontWeight w : {FontWeight::Regular, FontWeight::Medium, FontWeight::SemiBold, FontWeight::Bold}) {
        const FontFace* face = embedded_face(w);
        CHECK(face != nullptr);
        CHECK(face->units_per_em() > 0);
        CHECK(face->ascent_units() > 0);
        CHECK(face->descent_units() < 0);
        CHECK(face->ascent_units() - face->descent_units() >= face->units_per_em()); // tall enough for its em
        if (em == 0) em = face->units_per_em();
        CHECK(face->units_per_em() == em); // all weights share one em
    }
    CHECK(embedded_face(FontWeight::Bold) == embedded_face(FontWeight::SemiBold));
    CHECK(embedded_face(FontWeight::Regular) != embedded_face(FontWeight::Medium));
    const FontFace* r = embedded_face(FontWeight::Regular);
    CHECK(r->scale_for_em_pixels(static_cast<float>(r->units_per_em())) > 0.999f &&
           r->scale_for_em_pixels(static_cast<float>(r->units_per_em())) < 1.001f);
    std::cout << "test_faces_load_with_sane_metrics passed (units per em " << em << ")\n";
}

// ------------------------------------------------------------------------------------------------------------
void test_every_supported_code_point_has_a_glyph() {
    size_t checked = 0;
    for (FontWeight w : {FontWeight::Regular, FontWeight::Medium, FontWeight::SemiBold}) {
        const FontFace* face = embedded_face(w);
        for (const CodePointRange& range : kSupportedRanges) {
            for (char32_t cp = range.first; cp <= range.last; ++cp) {
                if (!face->has_glyph(cp)) std::cout << "  MISSING glyph U+" << std::hex << static_cast<unsigned>(cp) << std::dec << "\n";
                CHECK(face->has_glyph(cp));
                CHECK(face->advance_units(face->glyph_index(cp)) >= 0);
                ++checked;
            }
        }
    }
    CHECK(checked == 3 * supported_code_point_count());
    CHECK(supported_code_point_count() == 204); // 95 ASCII + 95 Latin-1 (no U+00AD) + 14 punctuation/symbols/arrows

    // Policy checks of the charset itself (not claims about what the font happens to contain).
    CHECK(is_supported_code_point(U'A') && is_supported_code_point(0x00E9) && is_supported_code_point(0x2026));
    CHECK(!is_supported_code_point(0x4E2D));   // CJK
    CHECK(!is_supported_code_point(0x1F600));  // emoji
    CHECK(!is_supported_code_point(0x0301));   // combining acute accent
    CHECK(!is_supported_code_point(0x05D0));   // Hebrew
    CHECK(!is_supported_code_point(0x0410));   // Cyrillic (Inter has it, but it is outside the declared support)
    CHECK(is_control_code_point(0x0A) && is_control_code_point(0x7F) && is_control_code_point(0x9F));
    CHECK(!is_control_code_point(0x20) && !is_control_code_point(0xA0));
    // Invisible format characters are neither supported glyphs nor controls, and the font indeed lacks U+00AD.
    for (char32_t cp : {char32_t{0x00AD}, char32_t{0x200B}, char32_t{0x200C}, char32_t{0x200D}, char32_t{0x2060}, char32_t{0xFEFF}}) {
        CHECK(is_ignorable_code_point(cp) && !is_supported_code_point(cp) && !is_control_code_point(cp));
    }
    CHECK(!is_ignorable_code_point(U'A') && !is_ignorable_code_point(0xA0) && !is_ignorable_code_point(0xFFFD));
    CHECK(!embedded_face(FontWeight::Regular)->has_glyph(0x00AD));
    // Inter has no U+FFFD either: the replacement is the face's .notdef glyph (index 0), as documented in text_charset.hpp.
    // If a future Inter release adds U+FFFD this fails on purpose, prompting a review of that policy.
    CHECK(!is_supported_code_point(0xFFFD));
    for (FontWeight w : {FontWeight::Regular, FontWeight::Medium, FontWeight::SemiBold}) {
        CHECK(!embedded_face(w)->has_glyph(0xFFFD));
    }
    CHECK(embedded_face(FontWeight::Regular)->glyph_index(0x110000) == 0);
    CHECK(embedded_face(FontWeight::Regular)->glyph_index(0xFFFFFFFF) == 0);
    std::cout << "test_every_supported_code_point_has_a_glyph passed (" << checked << " glyph lookups)\n";
}

// ------------------------------------------------------------------------------------------------------------
void test_advances_and_weights() {
    const FontFace* reg = embedded_face(FontWeight::Regular);
    const FontFace* med = embedded_face(FontWeight::Medium);
    const FontFace* semi = embedded_face(FontWeight::SemiBold);
    auto adv = [](const FontFace* f, char32_t cp) { return f->advance_units(f->glyph_index(cp)); };

    CHECK(adv(reg, U' ') > 0);
    CHECK(adv(reg, U'i') < adv(reg, U'n') && adv(reg, U'n') < adv(reg, U'M'));
    CHECK(adv(reg, U'l') < adv(reg, U'W'));

    // The three files really are different weights: heavier faces are never narrower, and at least some letters
    // are strictly wider.
    int strictly_wider = 0;
    for (char32_t cp : U"abcdeghnopqrsuxyzABEHKRSTZ") {
        if (cp == 0) break;
        CHECK(adv(med, cp) >= adv(reg, cp));
        CHECK(adv(semi, cp) >= adv(med, cp));
        if (adv(semi, cp) > adv(reg, cp)) ++strictly_wider;
    }
    CHECK(strictly_wider >= 10);

    // Advances are plausible proportions of the em (no kerning is applied; see font_face.hpp).
    for (char32_t cp : U"AaMWimn0 ") {
        if (cp == 0) break;
        const int a = adv(reg, cp);
        CHECK(a > 0 && a < reg->units_per_em() * 11 / 10);
    }
    // Tabular figures are not forced: digits may vary, but '0' is clearly narrower than 'M'.
    CHECK(adv(reg, U'0') < adv(reg, U'M'));
    std::cout << "test_advances_and_weights passed\n";
}

// ------------------------------------------------------------------------------------------------------------
struct Ink {
    int width = 0;
    int height = 0;
    FontFace::GlyphBox box{0, 0, 0, 0};
    std::vector<unsigned char> pixels;
    unsigned long long sum = 0;
    int max = 0;
    int partial = 0; // pixels that are neither empty nor fully covered
};

Ink render(const FontFace* face, char32_t cp, float px) {
    Ink ink;
    const float scale = face->scale_for_em_pixels(px);
    const int g = face->glyph_index(cp);
    ink.box = face->glyph_box(g, scale);
    ink.width = ink.box.width();
    ink.height = ink.box.height();
    if (ink.width == 0 || ink.height == 0) return ink;
    // Guarded buffer: any write past the reported box would corrupt the guards.
    const size_t guard = 64;
    std::vector<unsigned char> buf(guard * 2 + static_cast<size_t>(ink.width) * ink.height, 0xAB);
    std::fill(buf.begin() + guard, buf.end() - guard, static_cast<unsigned char>(0));
    face->rasterize(g, scale, buf.data() + guard, ink.width, ink.height, ink.width);
    for (size_t i = 0; i < guard; ++i) {
        CHECK(buf[i] == 0xAB && buf[buf.size() - 1 - i] == 0xAB);
    }
    ink.pixels.assign(buf.begin() + guard, buf.end() - guard);
    for (unsigned char p : ink.pixels) {
        ink.sum += p;
        ink.max = std::max<int>(ink.max, p);
        if (p != 0 && p != 255) ++ink.partial;
    }
    return ink;
}

void test_rasterization_basics() {
    const FontFace* reg = embedded_face(FontWeight::Regular);

    const Ink space = render(reg, U' ', 14.0f);
    CHECK(space.width == 0 || space.height == 0 || space.sum == 0); // space has no ink

    const Ink h = render(reg, U'H', 14.0f);
    CHECK(h.width >= 8 && h.width <= 12);
    CHECK(h.height >= 9 && h.height <= 12);          // cap height ~0.73 em = ~10 px, plus anti-aliasing fringe
    CHECK(h.box.y0 < 0 && h.box.y1 <= 1);            // sits on the baseline, rises above it
    CHECK(h.max >= 200);                             // the stems get close to full coverage
    CHECK(h.partial >= 8);                           // genuinely anti-aliased, not 1-bit

    const Ink x = render(reg, U'x', 14.0f);
    CHECK(x.height >= 6 && x.height <= 9);           // x-height ~0.55 em = ~7.6 px
    CHECK(x.box.y1 <= 1);                            // x rests on the baseline
    CHECK(x.height < h.height);                      // lower case is shorter than capitals

    const Ink p = render(reg, U'p', 14.0f);
    CHECK(p.box.y1 >= 3);                            // 'p' has a descender below the baseline
    const Ink l = render(reg, U'l', 14.0f);
    CHECK(l.height > x.height);                      // ascender taller than x-height

    // Rasterizing is deterministic.
    const Ink h2 = render(reg, U'H', 14.0f);
    CHECK(h.pixels == h2.pixels && h.box.x0 == h2.box.x0 && h.box.y0 == h2.box.y0);

    // Larger sizes scale up smoothly; the glyph grows in proportion.
    const Ink h32 = render(reg, U'H', 32.0f);
    CHECK(h32.height > h.height * 2 && h32.height < h.height * 3);

    // Heavier weights carry more ink for the same glyph and size.
    const Ink o_reg = render(embedded_face(FontWeight::Regular), U'o', 14.0f);
    const Ink o_med = render(embedded_face(FontWeight::Medium), U'o', 14.0f);
    const Ink o_semi = render(embedded_face(FontWeight::SemiBold), U'o', 14.0f);
    CHECK(o_reg.sum < o_med.sum && o_med.sum < o_semi.sum);
    CHECK(o_semi.sum > o_reg.sum * 1.10);
    std::cout << "  'o' ink at 14px  regular " << o_reg.sum << "  medium " << o_med.sum << "  semibold " << o_semi.sum << "\n";
    std::cout << "test_rasterization_basics passed\n";
}

void test_every_supported_glyph_renders_at_every_token_size() {
    // The sizes the atlas will be built for (theme typography tokens).
    const float sizes[] = {12.0f, 14.0f, 15.0f, 20.0f, 32.0f};
    size_t blank = 0;
    for (FontWeight w : {FontWeight::Regular, FontWeight::Medium, FontWeight::SemiBold}) {
        const FontFace* face = embedded_face(w);
        for (float px : sizes) {
            for (const CodePointRange& range : kSupportedRanges) {
                for (char32_t cp = range.first; cp <= range.last; ++cp) {
                    const Ink ink = render(face, cp, px);
                    const bool is_space = cp == U' ' || cp == 0xA0;
                    if (is_space) {
                        CHECK(ink.sum == 0);
                    } else {
                        if (ink.sum == 0) {
                            std::cout << "  BLANK U+" << std::hex << static_cast<unsigned>(cp) << std::dec << " at " << px << "px\n";
                            ++blank;
                        }
                        // Ink must be bounded by a sane box: nothing is wider or taller than ~1.5 em.
                        CHECK(ink.width <= static_cast<int>(px * 1.5f) + 2 && ink.height <= static_cast<int>(px * 1.5f) + 2);
                    }
                }
            }
            // The replacement glyph (glyph 0, .notdef, reached through U+FFFD) is visible and sensibly sized.
            const Ink box = render(face, 0xFFFD, px);
            std::cout << "  .notdef at " << px << "px: " << box.width << "x" << box.height << " box (" << box.box.x0 << "," << box.box.y0
                      << ")-(" << box.box.x1 << "," << box.box.y1 << ") ink " << box.sum << "\n";
            CHECK(box.sum > 0);
            // Inter's .notdef is a tall, narrow box (~0.35 em wide, ~1.2 em tall): unlike any letter, so a missing or
            // invalid character is unmistakable.
            CHECK(box.width >= static_cast<int>(px * 0.25f) && box.height >= static_cast<int>(px * 0.8f));
            CHECK(box.width <= static_cast<int>(px * 0.6f) + 2 && box.height <= static_cast<int>(px * 1.3f) + 2);
        }
    }
    CHECK(blank == 0);
    std::cout << "test_every_supported_glyph_renders_at_every_token_size passed\n";
}

} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    // Failed assertions must print to stderr and exit, never open a modal dialog that would hang an unattended run.
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    if (argc < 2) {
        std::cerr << "usage: test_font_face <font asset directory>\n";
        return 2;
    }
    g_font_dir = argv[1];
    std::cout << "Running Font Face Tests...\n";
    test_embedded_bytes_match_audited_assets();
    test_validate_accepts_real_fonts_and_rejects_garbage();
    test_faces_load_with_sane_metrics();
    test_every_supported_code_point_has_a_glyph();
    test_advances_and_weights();
    test_rasterization_basics();
    test_every_supported_glyph_renders_at_every_token_size();
    std::cout << "All font face tests passed successfully.\n";
    return 0;
}
