#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include "vynexos/desktop/theme.hpp"

namespace vynexos::desktop {

/**
 * @brief A parsed TrueType face (wraps stb_truetype; no stb types appear in this header).
 *
 * SECURITY: stb_truetype performs no range checking on offsets inside a font file and must never be fed untrusted
 * data. VynexOS therefore only ever constructs faces from the audited fonts embedded in the executable
 * (see embedded_face()). create() additionally rejects structurally implausible input, as defence in depth - it is
 * not a substitute for that rule, and no API here loads fonts from disk or the network.
 *
 * All query methods are const, allocation-free and thread-safe. rasterize() allocates temporary working memory
 * inside stb and is meant for one-time atlas construction only, never for per-frame use.
 */
class FontFace {
public:
    ~FontFace();
    FontFace(FontFace&&) noexcept;
    FontFace& operator=(FontFace&&) noexcept;
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;

    /// Structural plausibility check of a TrueType/OpenType file: sfnt version, table directory bounds, presence and
    /// minimum size of the tables the renderer needs. Does not look inside the tables.
    [[nodiscard]] static bool validate(std::span<const unsigned char> data) noexcept;

    /// Returns null when `data` fails validate() or stb cannot initialise it. `data` must outlive the face.
    [[nodiscard]] static std::unique_ptr<FontFace> create(std::span<const unsigned char> data);

    // --- Font-wide metrics, in font units (divide by units_per_em() * size to get pixels) ---
    [[nodiscard]] int units_per_em() const noexcept;
    [[nodiscard]] int ascent_units() const noexcept;   ///< positive, above the baseline
    [[nodiscard]] int descent_units() const noexcept;  ///< negative, below the baseline
    [[nodiscard]] int line_gap_units() const noexcept;

    /// Pixel scale such that one em equals `em_pixels` (i.e. a "14px font").
    [[nodiscard]] float scale_for_em_pixels(float em_pixels) const noexcept;

    // --- Per-glyph queries ---
    /// Glyph index for a code point, or 0 (the .notdef glyph) when the face has no glyph for it.
    [[nodiscard]] int glyph_index(char32_t code_point) const noexcept;
    [[nodiscard]] bool has_glyph(char32_t code_point) const noexcept { return glyph_index(code_point) != 0; }
    [[nodiscard]] int advance_units(int glyph) const noexcept;
    // Kerning is intentionally NOT offered. stb_truetype 1.26 reads only direct GPOS pair-adjustment lookups, but
    // Inter stores its kerning in an Extension lookup (type 9) that stb skips, so every pair would report 0. Text is
    // laid out with plain advance widths (Inter's designed default spacing). A GPOS reader would be a separate,
    // reviewed change; see text_charset.hpp / the Stage 2 notes.

    /// Bounding box of the rendered glyph in pixels, relative to the glyph origin on the baseline (y grows down).
    /// Empty glyphs (space) yield x1 <= x0 or y1 <= y0.
    struct GlyphBox {
        int x0, y0, x1, y1;
        [[nodiscard]] int width() const noexcept { return x1 > x0 ? x1 - x0 : 0; }
        [[nodiscard]] int height() const noexcept { return y1 > y0 ? y1 - y0 : 0; }
    };
    [[nodiscard]] GlyphBox glyph_box(int glyph, float scale) const noexcept;

    /// Renders the glyph's anti-aliased coverage (0..255) into `out`, which must hold at least `height * stride`
    /// bytes for the box returned by glyph_box() (width <= stride). Atlas construction only: allocates temporarily.
    void rasterize(int glyph, float scale, unsigned char* out, int width, int height, int stride) const noexcept;

private:
    struct Impl;
    explicit FontFace(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

/// The process-wide face for a weight, built from the embedded Inter font on first use (thread-safe). Bold maps to
/// SemiBold. Returns null only if the embedded data fails validation, which would be a build error.
[[nodiscard]] const FontFace* embedded_face(FontWeight weight) noexcept;

} // namespace vynexos::desktop
