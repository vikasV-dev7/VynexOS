#pragma once
#include <array>
#include <cstddef>

namespace vynexos::desktop {

/**
 * @file text_charset.hpp
 * @brief The explicit set of characters the VynexOS text renderer supports.
 *
 * SUPPORTED (rendered with real, anti-aliased glyphs from the embedded Inter fonts):
 *   - Basic Latin             U+0020..U+007E   (space and printable ASCII)
 *   - Latin-1 Supplement      U+00A0..U+00FF   except U+00AD (see below): Western European letters, punctuation,
 *                             currency and signs
 *   - General punctuation     U+2013 en dash, U+2014 em dash, U+2018/U+2019 single quotes,
 *                             U+201C/U+201D double quotes, U+2022 bullet, U+2026 ellipsis
 *   - Currency / math / UI    U+20AC euro sign, U+2212 minus sign, U+2190..U+2193 arrows (left, up, right, down)

 * REPLACEMENT GLYPH. Inter 4.1 has no glyph for U+FFFD, so ill-formed UTF-8 and unsupported code points are drawn
 * with the font's own .notdef glyph (glyph index 0, an empty box). It is visibly different from every real
 * character, so a missing or invalid character is never silently misrepresented as something else.
 *
 * NOT SUPPORTED. Each unsupported code point is drawn as that replacement box, and the limits below are part of the
 * contract:
 *   - Other scripts: Cyrillic, Greek, Arabic, Hebrew, Devanagari, Thai, CJK (Chinese/Japanese/Korean), ...
 *   - Combining marks and any sequence that relies on them (a combining accent is drawn as the replacement box, not
 *     applied to the previous letter). Use precomposed Latin-1 letters instead.
 *   - Right-to-left and bidirectional text, text shaping, ligatures, contextual forms.
 *   - Emoji and pictographs (including variation selectors and zero-width joiners).
 *   - Control characters (U+0000..U+001F, U+007F..U+009F): not drawn and have zero width. Newlines are NOT
 *     interpreted: draw one line per call.
 *
 * INVISIBLE BY DEFINITION. These "default-ignorable" format characters have no visible form (and Inter has no glyph
 * for U+00AD), so they are neither drawn nor replaced by the box; like control characters they have zero width:
 *   U+00AD soft hyphen, U+200B zero-width space, U+200C/U+200D zero-width (non-)joiner, U+2060 word joiner,
 *   U+FEFF byte-order mark / zero-width no-break space.
 *
 * Sizes are limited to the typography tokens in theme.hpp (caption, body, body_strong, title, heading, display).
 */

struct CodePointRange {
    char32_t first;
    char32_t last; ///< inclusive
};

/// Sorted, non-overlapping ranges of supported code points.
inline constexpr std::array<CodePointRange, 11> kSupportedRanges{{
    {0x0020, 0x007E},
    {0x00A0, 0x00AC},
    {0x00AE, 0x00FF}, // U+00AD (soft hyphen) is invisible, see is_ignorable_code_point()
    {0x2013, 0x2014},
    {0x2018, 0x2019},
    {0x201C, 0x201D},
    {0x2022, 0x2022},
    {0x2026, 0x2026},
    {0x20AC, 0x20AC},
    {0x2190, 0x2193},
    {0x2212, 0x2212},
}};

/// True when `cp` has a real glyph in the text renderer.
[[nodiscard]] constexpr bool is_supported_code_point(char32_t cp) noexcept {
    for (const CodePointRange& r : kSupportedRanges) {
        if (cp >= r.first && cp <= r.last) return true;
    }
    return false;
}

/// Control characters: skipped by the renderer, zero width.
[[nodiscard]] constexpr bool is_control_code_point(char32_t cp) noexcept {
    return cp < 0x20 || (cp >= 0x7F && cp < 0xA0);
}

/// Invisible format characters: not drawn, zero width, and deliberately not replaced by U+FFFD.
[[nodiscard]] constexpr bool is_ignorable_code_point(char32_t cp) noexcept {
    return cp == 0x00AD || (cp >= 0x200B && cp <= 0x200D) || cp == 0x2060 || cp == 0xFEFF;
}

/// Number of supported code points (the ranges are disjoint, so their lengths simply add up).
[[nodiscard]] constexpr size_t supported_code_point_count() noexcept {
    size_t n = 0;
    for (const CodePointRange& r : kSupportedRanges) n += static_cast<size_t>(r.last - r.first) + 1;
    return n;
}

} // namespace vynexos::desktop
