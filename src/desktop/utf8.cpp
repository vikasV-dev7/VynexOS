#include "vynexos/desktop/utf8.hpp"

namespace vynexos::desktop::utf8 {

namespace {

[[nodiscard]] constexpr bool in_range(unsigned char b, unsigned char lo, unsigned char hi) noexcept {
    return b >= lo && b <= hi;
}

} // namespace

Decoded decode_next(std::string_view bytes) noexcept {
    if (bytes.empty()) return Decoded{kReplacement, 0, false}; // contract violation: nothing to consume

    const auto lead = static_cast<unsigned char>(bytes[0]);
    if (lead < 0x80) return Decoded{lead, 1, true};

    // Per Unicode Table 3-7: the lead byte fixes the sequence length and the valid range of the *second* byte; every
    // later byte must be 0x80..0xBF.
    unsigned need = 0;          // continuation bytes required after the lead byte
    unsigned char lo = 0x80;    // allowed range of the first continuation byte
    unsigned char hi = 0xBF;
    char32_t cp = 0;

    if (in_range(lead, 0xC2, 0xDF)) {
        need = 1;
        cp = lead & 0x1Fu;
    } else if (in_range(lead, 0xE0, 0xEF)) {
        need = 2;
        cp = lead & 0x0Fu;
        if (lead == 0xE0) lo = 0xA0; // reject overlong 3-byte forms
        if (lead == 0xED) hi = 0x9F; // reject surrogates
    } else if (in_range(lead, 0xF0, 0xF4)) {
        need = 3;
        cp = lead & 0x07u;
        if (lead == 0xF0) lo = 0x90; // reject overlong 4-byte forms
        if (lead == 0xF4) hi = 0x8F; // reject values above U+10FFFF
    } else {
        return Decoded{kReplacement, 1, false}; // 0x80..0xC1 and 0xF5..0xFF can never start a sequence
    }

    unsigned consumed = 1;
    for (unsigned i = 0; i < need; ++i) {
        if (consumed >= bytes.size()) return Decoded{kReplacement, consumed, false}; // truncated
        const auto b = static_cast<unsigned char>(bytes[consumed]);
        const unsigned char min = (i == 0) ? lo : static_cast<unsigned char>(0x80);
        const unsigned char max = (i == 0) ? hi : static_cast<unsigned char>(0xBF);
        if (!in_range(b, min, max)) return Decoded{kReplacement, consumed, false}; // maximal subpart so far
        cp = (cp << 6) | (b & 0x3Fu);
        ++consumed;
    }
    return Decoded{cp, consumed, true};
}

size_t count_code_points(std::string_view text) noexcept {
    size_t count = 0;
    while (!text.empty()) {
        text.remove_prefix(decode_next(text).length);
        ++count;
    }
    return count;
}

bool is_valid(std::string_view text) noexcept {
    while (!text.empty()) {
        const Decoded d = decode_next(text);
        if (!d.valid) return false;
        text.remove_prefix(d.length);
    }
    return true;
}

size_t encode(char32_t cp, char* out) noexcept {
    if (cp < 0x80) {
        out[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        out[0] = static_cast<char>(0xC0u | (cp >> 6));
        out[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0; // surrogates are not scalar values
    if (cp < 0x10000) {
        out[0] = static_cast<char>(0xE0u | (cp >> 12));
        out[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out[0] = static_cast<char>(0xF0u | (cp >> 18));
        out[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 4;
    }
    return 0;
}

} // namespace vynexos::desktop::utf8
