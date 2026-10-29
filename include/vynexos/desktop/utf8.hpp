#pragma once
#include <cstddef>
#include <string_view>

namespace vynexos::desktop::utf8 {

/// U+FFFD REPLACEMENT CHARACTER, substituted for every ill-formed byte sequence.
inline constexpr char32_t kReplacement = 0xFFFD;

struct Decoded {
    char32_t code_point;  ///< The decoded scalar value, or kReplacement when `valid` is false.
    unsigned length;      ///< Bytes consumed (always >= 1 for non-empty input).
    bool valid;           ///< False when the bytes were ill-formed and replaced.
};

/// Decodes the code point at the start of `bytes`, which must not be empty.
///
/// Strict UTF-8 (RFC 3629 / Unicode Table 3-7): overlong forms, UTF-16 surrogates (U+D800..U+DFFF), values above
/// U+10FFFF, stray continuation bytes and truncated sequences are all rejected. For ill-formed input the decoder
/// consumes the "maximal subpart" of the ill-formed sequence (Unicode's recommended practice), so the number of
/// replacement characters is well defined and decoding always makes progress. Never reads past the view.
[[nodiscard]] Decoded decode_next(std::string_view bytes) noexcept;

/// Number of code points `text` decodes to (each ill-formed subpart counts once, as its replacement).
[[nodiscard]] size_t count_code_points(std::string_view text) noexcept;

/// True when `text` is entirely well-formed UTF-8.
[[nodiscard]] bool is_valid(std::string_view text) noexcept;

/// Encodes a scalar value into `out` (at least 4 bytes). Returns the byte count, or 0 for surrogates and values
/// above U+10FFFF (which are not scalar values).
[[nodiscard]] size_t encode(char32_t code_point, char* out) noexcept;

} // namespace vynexos::desktop::utf8
