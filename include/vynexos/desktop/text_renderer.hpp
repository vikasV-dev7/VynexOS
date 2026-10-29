#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/theme.hpp"

namespace vynexos::desktop {

class Painter;

/// Result of measuring a run of text.
struct TextMetrics {
    float width{0.0f};       ///< Sum of glyph advances in pixels (the pen travel; ink may overhang by a pixel or so).
    float ascent{0.0f};      ///< Pixels above the baseline reserved by the font (positive).
    float descent{0.0f};     ///< Pixels below the baseline reserved by the font (positive).
    float line_height{0.0f}; ///< The style's line height in pixels.
    size_t glyphs{0};        ///< Glyphs measured (control and invisible characters are not counted).
};

/// Vertical metrics of a style, for aligning text inside boxes.
struct FontMetrics {
    float ascent{0.0f};
    float descent{0.0f};
    float line_height{0.0f};
    float cap_height{0.0f}; ///< Height of a capital 'H' above the baseline.
    float x_height{0.0f};   ///< Height of a lower-case 'x' above the baseline.
};

struct TextRendererOptions {
    /// Coverage curve applied once when the atlas is built: coverage' = coverage^(1/gamma). 1.0 keeps the rasterizer's
    /// linear coverage; values above 1 add weight, which helps light text on dark backgrounds stay crisp.
    float coverage_gamma{1.0f};
};

/**
 * @brief Anti-aliased UTF-8 text from the embedded Inter fonts.
 *
 * Construction rasterizes every supported glyph (see text_charset.hpp) for each distinct size/weight among the theme's
 * typography tokens into compact 8-bit coverage "strikes". After that, measuring and drawing perform **no heap
 * allocation** and the object is immutable, so all methods are const and thread-safe.
 *
 * Layout rules
 *  - Text is a single line. Control characters (including newline and tab) and invisible format characters have zero
 *    width and are not drawn.
 *  - Ill-formed UTF-8 and unsupported code points (other scripts, combining marks, emoji, ...) are drawn as the font's
 *    replacement box; see text_charset.hpp for the exact supported set.
 *  - Glyphs advance by their designed widths (no kerning: stb_truetype cannot read Inter's GPOS kerning). Each glyph is
 *    placed at a whole pixel while the fractional pen position is carried along, so spacing error never accumulates.
 *  - Sizes come from the typography tokens. A style that matches none exactly uses the nearest strike by size, then by
 *    weight (Bold maps to SemiBold, the heaviest embedded weight).
 */
class TextRenderer {
public:
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    /// Builds the glyph strikes. Returns null if the embedded fonts cannot be used (which would be a build error).
    [[nodiscard]] static std::unique_ptr<TextRenderer> create(const TypographyTokens& tokens = default_theme().type,
                                                              const TextRendererOptions& options = {});

    /// The process-wide renderer for the default theme's typography, built on first call (thread-safe) and then shared:
    /// the atlas is only a few hundred kilobytes and immutable, so every toolkit instance can use the same one.
    /// Call it during start-up so the one-time build never lands inside a frame. Null if construction failed.
    [[nodiscard]] static const TextRenderer* shared_default();

    [[nodiscard]] TextMetrics measure(std::string_view utf8, const TextStyle& style) const noexcept;
    [[nodiscard]] FontMetrics font_metrics(const TextStyle& style) const noexcept;

    /// Draws `utf8` with the pen starting at (x, baseline_y). Returns the advance width (== measure().width).
    float draw(Painter& painter, float x, int32_t baseline_y, std::string_view utf8, const TextStyle& style, Color color) const noexcept;

    /// Number of bytes of the longest prefix of `utf8` whose width is <= max_width. Never splits a UTF-8 sequence.
    [[nodiscard]] size_t fit_prefix(std::string_view utf8, const TextStyle& style, float max_width) const noexcept;

    /// Draws `utf8`, shortened with an ellipsis (U+2026) when it would be wider than max_width. The drawn width never
    /// exceeds max_width. Returns the width actually drawn (0 if not even the ellipsis fits).
    float draw_ellipsized(Painter& painter, float x, int32_t baseline_y, std::string_view utf8, const TextStyle& style, Color color,
                          float max_width) const noexcept;

    // --- Introspection (tests and diagnostics) ---
    [[nodiscard]] size_t strike_count() const noexcept;
    [[nodiscard]] size_t atlas_bytes() const noexcept;  ///< Total bytes of coverage data across all strikes.
    [[nodiscard]] size_t glyphs_per_strike() const noexcept;

private:
    struct Impl;
    explicit TextRenderer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vynexos::desktop
