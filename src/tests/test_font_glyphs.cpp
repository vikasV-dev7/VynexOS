#include <array>
#include "test_check.hpp"
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/software_surface.hpp"

// The bitmap table is defined (non-static) in the toolkit translation unit. It is declared here
// instead of including font8x8_basic.h, which would define it a second time.
extern unsigned char font8x8_basic[128][8];

using namespace vynexos::desktop;

namespace {

using Bitmap = std::array<std::string, 8>; // 8 rows of '#' / '.'

// Renders one character through the real toolkit path onto a transparent 8x8 surface.
Bitmap render_glyph(BasicWidgetToolkit& toolkit, char c) {
    auto surface = std::make_shared<SoftwareSurface>(8, 8);
    toolkit.draw_text_bitmap(surface, 0, 0, std::string(1, c), Color{255, 255, 255, 255});
    auto px = surface->map_pixels();
    Bitmap bm;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const size_t idx = (static_cast<size_t>(row) * 8 + static_cast<size_t>(col)) * 4;
            bm[static_cast<size_t>(row)].push_back(px[idx + 3] != 0 ? '#' : '.');
        }
    }
    surface->unmap_pixels();
    return bm;
}

// Independent decode of the table: bit 0 is the LEFTMOST pixel (font8x8 convention).
Bitmap decode_table(char c) {
    Bitmap bm;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const bool on = (font8x8_basic[static_cast<uint8_t>(c)][row] >> col) & 1;
            bm[static_cast<size_t>(row)].push_back(on ? '#' : '.');
        }
    }
    return bm;
}

bool is_blank(const Bitmap& bm) {
    for (const auto& r : bm) {
        if (r.find('#') != std::string::npos) return false;
    }
    return true;
}

void print_text_dump(BasicWidgetToolkit& toolkit, const std::string& text) {
    const auto w = static_cast<uint32_t>(text.size() * 8);
    auto surface = std::make_shared<SoftwareSurface>(w, 8);
    toolkit.draw_text_bitmap(surface, 0, 0, text, Color{255, 255, 255, 255});
    auto px = surface->map_pixels();
    std::cout << "  \"" << text << "\" rendered at 1:1:\n";
    for (uint32_t row = 0; row < 8; ++row) {
        std::cout << "    ";
        for (uint32_t col = 0; col < w; ++col) {
            std::cout << (px[(row * w + col) * 4 + 3] ? '#' : '.');
        }
        std::cout << '\n';
    }
    surface->unmap_pixels();
}

} // namespace

void test_render_matches_table_for_all_printable() {
    BasicWidgetToolkit toolkit;
    for (int c = 0x20; c <= 0x7E; ++c) {
        const auto rendered = render_glyph(toolkit, static_cast<char>(c));
        const auto expected = decode_table(static_cast<char>(c));
        CHECK(rendered == expected);
    }
    std::cout << "test_render_matches_table_for_all_printable passed\n";
}

void test_known_glyph_golden() {
    BasicWidgetToolkit toolkit;
    // 'A' in the public-domain font8x8 table: 0C 1E 33 33 3F 33 33 00 (bit 0 = leftmost pixel).
    const Bitmap golden_A{"..##....", ".####...", "##..##..", "##..##..",
                          "######..", "##..##..", "##..##..", "........"};
    CHECK(render_glyph(toolkit, 'A') == golden_A);
    std::cout << "test_known_glyph_golden passed\n";
}

void test_printable_glyphs_present_and_distinct() {
    BasicWidgetToolkit toolkit;
    std::set<std::string> seen;
    std::vector<char> duplicates;
    std::vector<char> blanks;
    for (int c = 0x21; c <= 0x7E; ++c) {
        const auto bm = render_glyph(toolkit, static_cast<char>(c));
        if (is_blank(bm)) blanks.push_back(static_cast<char>(c));
        std::string key;
        for (const auto& r : bm) key += r;
        if (!seen.insert(key).second) duplicates.push_back(static_cast<char>(c));
    }
    for (char c : blanks) std::cout << "  BLANK glyph: '" << c << "'\n";
    for (char c : duplicates) std::cout << "  DUPLICATE glyph: '" << c << "'\n";
    CHECK(blanks.empty());
    CHECK(duplicates.empty());

    // Space is blank, as it should be.
    CHECK(is_blank(render_glyph(toolkit, ' ')));
    std::cout << "test_printable_glyphs_present_and_distinct passed\n";
}

void test_symmetric_glyphs_are_not_mirrored_or_corrupt() {
    BasicWidgetToolkit toolkit;
    // Letters/digits that are mirror-symmetric about their own bounding box in a correct font.
    // A wrong bit order or a corrupted row breaks this.
    // ('0' is excluded on purpose: this font draws a slashed zero.)
    const std::string symmetric = "AHIMOTUVWXY8";
    for (char c : symmetric) {
        const auto bm = render_glyph(toolkit, c);
        int min_col = 8;
        int max_col = -1;
        for (const auto& r : bm) {
            for (int col = 0; col < 8; ++col) {
                if (r[static_cast<size_t>(col)] == '#') {
                    min_col = std::min(min_col, col);
                    max_col = std::max(max_col, col);
                }
            }
        }
        CHECK(max_col >= min_col);
        for (const auto& r : bm) {
            for (int col = min_col; col <= max_col; ++col) {
                const int mirror = max_col - (col - min_col);
                if (r[static_cast<size_t>(col)] != r[static_cast<size_t>(mirror)]) {
                    std::cout << "  glyph '" << c << "' is not symmetric:\n";
                    for (const auto& rr : bm) std::cout << "    " << rr << '\n';
                }
                CHECK(r[static_cast<size_t>(col)] == r[static_cast<size_t>(mirror)]);
            }
        }
    }
    std::cout << "test_symmetric_glyphs_are_not_mirrored_or_corrupt passed\n";
}

void test_text_advance_and_clipping() {
    BasicWidgetToolkit toolkit;
    // Fixed 8px advance: "AA" must equal two 'A' glyphs side by side.
    auto surface = std::make_shared<SoftwareSurface>(16, 8);
    toolkit.draw_text_bitmap(surface, 0, 0, "AA", Color{255, 255, 255, 255});
    auto px = surface->map_pixels();
    const auto a = render_glyph(toolkit, 'A');
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 16; ++col) {
            const bool on = px[(static_cast<size_t>(row) * 16 + static_cast<size_t>(col)) * 4 + 3] != 0;
            const bool expected = a[static_cast<size_t>(row)][static_cast<size_t>(col % 8)] == '#';
            CHECK(on == expected);
        }
    }
    surface->unmap_pixels();

    // Drawing partly or fully outside the surface must not crash or corrupt memory.
    auto small = std::make_shared<SoftwareSurface>(4, 4);
    toolkit.draw_text_bitmap(small, -6, -6, "Hello", Color{255, 255, 255, 255});
    toolkit.draw_text_bitmap(small, 100, 100, "Hello", Color{255, 255, 255, 255});
    std::cout << "test_text_advance_and_clipping passed\n";
}

int main() {
    std::cout << std::unitbuf; // keep diagnostics visible if an assertion aborts
    std::cout << "Running Font Glyph Tests...\n";
    BasicWidgetToolkit toolkit;
    print_text_dump(toolkit, "Application");
    print_text_dump(toolkit, "root@vynex:~#");

    test_render_matches_table_for_all_printable();
    test_known_glyph_golden();
    test_printable_glyphs_present_and_distinct();
    test_symmetric_glyphs_are_not_mirrored_or_corrupt();
    test_text_advance_and_clipping();
    std::cout << "All font glyph tests passed successfully.\n";
    return 0;
}
