// Strict UTF-8 decoding tests: well-formed input, every class of ill-formed input, Unicode's published conformance
// example for U+FFFD substitution, an exhaustive round trip over all scalar values, and a guarded-buffer fuzz.
#include "test_check.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include "vynexos/desktop/utf8.hpp"

using namespace vynexos::desktop::utf8;

namespace {

// Decodes everything into code points (replacement for each ill-formed subpart).
std::vector<char32_t> decode_all(std::string_view s) {
    std::vector<char32_t> out;
    while (!s.empty()) {
        const Decoded d = decode_next(s);
        CHECK(d.length >= 1 && d.length <= s.size()); // always progresses, never over-reads
        out.push_back(d.code_point);
        s.remove_prefix(d.length);
    }
    return out;
}

std::string bytes(std::initializer_list<unsigned> list) {
    std::string s;
    for (unsigned b : list) s.push_back(static_cast<char>(b));
    return s;
}

constexpr char32_t R = kReplacement;

void test_ascii_and_boundaries() {
    const Decoded a = decode_next("A");
    CHECK(a.valid && a.code_point == U'A' && a.length == 1);
    const Decoded nul = decode_next(std::string_view("\0", 1));
    CHECK(nul.valid && nul.code_point == 0 && nul.length == 1); // embedded NUL is a valid code point

    // One representative at each length boundary.
    CHECK((decode_all(bytes({0xC2, 0x80})) == std::vector<char32_t>{0x80}));          // smallest 2-byte
    CHECK((decode_all(bytes({0xDF, 0xBF})) == std::vector<char32_t>{0x7FF}));         // largest 2-byte
    CHECK((decode_all(bytes({0xE0, 0xA0, 0x80})) == std::vector<char32_t>{0x800}));   // smallest 3-byte
    CHECK((decode_all(bytes({0xED, 0x9F, 0xBF})) == std::vector<char32_t>{0xD7FF}));  // just below surrogates
    CHECK((decode_all(bytes({0xEE, 0x80, 0x80})) == std::vector<char32_t>{0xE000}));  // just above surrogates
    CHECK((decode_all(bytes({0xEF, 0xBF, 0xBD})) == std::vector<char32_t>{0xFFFD}));  // U+FFFD itself
    CHECK((decode_all(bytes({0xEF, 0xBF, 0xBF})) == std::vector<char32_t>{0xFFFF}));
    CHECK((decode_all(bytes({0xF0, 0x90, 0x80, 0x80})) == std::vector<char32_t>{0x10000}));   // smallest 4-byte
    CHECK((decode_all(bytes({0xF4, 0x8F, 0xBF, 0xBF})) == std::vector<char32_t>{0x10FFFF}));  // largest scalar value

    // Real text: "Grüße € →" (ü, ß are Latin-1; € and → are in the supported general-punctuation set).
    const std::string text = "Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC \xE2\x86\x92";
    CHECK(is_valid(text));
    CHECK(count_code_points(text) == 9);
    const auto cps = decode_all(text);
    CHECK(cps[2] == 0xFC && cps[3] == 0xDF && cps[6] == 0x20AC && cps[8] == 0x2192);
    std::cout << "test_ascii_and_boundaries passed\n";
}

void test_unicode_conformance_example() {
    // Unicode "Table 3-8. Use of U+FFFD in UTF-8 Conversion": the recommended "maximal subpart" substitution.
    //   in : 61 F1 80 80 E1 80 C2 62 80 63 80 BF 64
    //   out: U+0061 FFFD FFFD FFFD U+0062 FFFD U+0063 FFFD FFFD U+0064
    const auto out = decode_all(bytes({0x61, 0xF1, 0x80, 0x80, 0xE1, 0x80, 0xC2, 0x62, 0x80, 0x63, 0x80, 0xBF, 0x64}));
    const std::vector<char32_t> expected{0x61, R, R, R, 0x62, R, 0x63, R, R, 0x64};
    CHECK(out == expected);
    std::cout << "test_unicode_conformance_example passed\n";
}

void test_ill_formed_classes() {
    // Stray continuation bytes: each is its own replacement.
    CHECK((decode_all(bytes({0x80})) == std::vector<char32_t>{R}));
    CHECK((decode_all(bytes({0xBF, 0x80})) == std::vector<char32_t>{R, R}));
    // Bytes that can never appear in UTF-8.
    CHECK((decode_all(bytes({0xC0, 0xAF})) == std::vector<char32_t>{R, R}));  // overlong '/' (C0 is never valid)
    CHECK((decode_all(bytes({0xC1, 0xBF})) == std::vector<char32_t>{R, R}));  // overlong (C1 is never valid)
    CHECK((decode_all(bytes({0xF5, 0x80, 0x80, 0x80})) == std::vector<char32_t>{R, R, R, R}));
    CHECK((decode_all(bytes({0xFE, 0xFF})) == std::vector<char32_t>{R, R}));
    // Overlong 3- and 4-byte forms.
    CHECK((decode_all(bytes({0xE0, 0x80, 0x80})) == std::vector<char32_t>{R, R, R}));
    CHECK((decode_all(bytes({0xE0, 0x9F, 0xBF})) == std::vector<char32_t>{R, R, R}));
    CHECK((decode_all(bytes({0xF0, 0x80, 0x80, 0x80})) == std::vector<char32_t>{R, R, R, R}));
    CHECK((decode_all(bytes({0xF0, 0x8F, 0xBF, 0xBF})) == std::vector<char32_t>{R, R, R, R}));
    // UTF-16 surrogates encoded as UTF-8 (CESU-style) are ill-formed.
    CHECK((decode_all(bytes({0xED, 0xA0, 0x80})) == std::vector<char32_t>{R, R, R})); // U+D800
    CHECK((decode_all(bytes({0xED, 0xBF, 0xBF})) == std::vector<char32_t>{R, R, R})); // U+DFFF
    // Above U+10FFFF.
    CHECK((decode_all(bytes({0xF4, 0x90, 0x80, 0x80})) == std::vector<char32_t>{R, R, R, R}));
    // Truncated sequences: the maximal valid prefix is one replacement.
    CHECK((decode_all(bytes({0xE2, 0x82})) == std::vector<char32_t>{R}));
    CHECK((decode_all(bytes({0xE2})) == std::vector<char32_t>{R}));
    CHECK((decode_all(bytes({0xF0, 0x9F, 0x98})) == std::vector<char32_t>{R}));
    CHECK((decode_all(bytes({0xC3})) == std::vector<char32_t>{R}));
    // Truncated sequence followed by ASCII: the ASCII byte must survive.
    CHECK((decode_all(bytes({0xE2, 0x82, 'A'})) == std::vector<char32_t>{R, U'A'}));
    CHECK((decode_all(bytes({0xC3, 'B'})) == std::vector<char32_t>{R, U'B'}));
    // Lead byte followed by another lead byte.
    CHECK((decode_all(bytes({0xE2, 0xC3, 0xBC})) == std::vector<char32_t>{R, 0xFC}));

    // valid flag and lengths for an ill-formed prefix.
    const std::string trunc = bytes({0xE2, 0x82});
    const Decoded d = decode_next(trunc);
    CHECK(!d.valid && d.length == 2 && d.code_point == R);
    CHECK(!is_valid(trunc));
    CHECK(count_code_points(trunc) == 1);
    std::cout << "test_ill_formed_classes passed\n";
}

void test_empty_and_encode_limits() {
    CHECK(count_code_points("") == 0);
    CHECK(is_valid(""));
    const Decoded e = decode_next(std::string_view{});
    CHECK(e.length == 0 && !e.valid); // documented: callers must not pass empty input

    char buf[4];
    CHECK(encode(0xD800, buf) == 0 && encode(0xDFFF, buf) == 0);   // surrogates are not scalar values
    CHECK(encode(0x110000, buf) == 0 && encode(0xFFFFFFFF, buf) == 0);
    CHECK(encode(0x10FFFF, buf) == 4);
    std::cout << "test_empty_and_encode_limits passed\n";
}

void test_exhaustive_round_trip() {
    // Every Unicode scalar value encodes to the canonical length and decodes back to itself.
    size_t checked = 0;
    for (char32_t cp = 0; cp <= 0x10FFFF; ++cp) {
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        char buf[4];
        const size_t n = encode(cp, buf);
        const size_t expected_len = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        CHECK(n == expected_len);
        const Decoded d = decode_next(std::string_view(buf, n));
        CHECK(d.valid && d.code_point == cp && d.length == n);
        // A truncated copy of any multi-byte sequence is exactly one replacement.
        if (n > 1) {
            const Decoded t = decode_next(std::string_view(buf, n - 1));
            CHECK(!t.valid && t.length == n - 1 && t.code_point == R);
        }
        ++checked;
    }
    CHECK(checked == 0x110000 - 0x800);
    std::cout << "test_exhaustive_round_trip passed (" << checked << " scalar values)\n";
}

void test_fuzz_never_overreads_and_always_progresses() {
    // Deterministic pseudo-random byte soup, decoded from the very end of a tightly sized heap buffer so that any
    // over-read is caught by the debug heap / sanitizers and by the explicit length assertions.
    uint64_t state = 0x9E3779B97F4A7C15ULL;
    auto next = [&]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned>(state >> 33);
    };
    size_t total_valid = 0;
    for (int round = 0; round < 20000; ++round) {
        const size_t len = next() % 12;
        std::vector<char> buf(len);
        for (char& c : buf) {
            // Bias toward interesting bytes: lead bytes, continuation bytes and boundary values.
            const unsigned pick = next() % 8;
            const unsigned char table[] = {0x00, 0x41, 0x7F, 0x80, 0xBF, 0xC2, 0xE0, 0xED, 0xF0, 0xF4, 0xF5, 0xFF, 0xA0, 0x90, 0x8F};
            c = static_cast<char>(pick < 5 ? table[next() % sizeof table] : static_cast<unsigned char>(next()));
        }
        std::string_view view(buf.data(), buf.size());
        size_t consumed = 0;
        while (consumed < buf.size()) {
            const Decoded d = decode_next(view.substr(consumed));
            CHECK(d.length >= 1 && consumed + d.length <= buf.size());
            if (d.valid) {
                // A valid decode must re-encode to exactly the bytes consumed (canonical form only).
                char enc[4];
                const size_t n = encode(d.code_point, enc);
                CHECK(n == d.length && std::string_view(enc, n) == view.substr(consumed, d.length));
                ++total_valid;
            } else {
                CHECK(d.code_point == R);
            }
            consumed += d.length;
        }
    }
    CHECK(total_valid > 1000); // the fuzz really did exercise valid sequences too
    std::cout << "test_fuzz_never_overreads_and_always_progresses passed\n";
}

} // namespace

int main() {
    std::cout << std::unitbuf;
    std::cout << "Running UTF-8 Tests...\n";
    test_ascii_and_boundaries();
    test_unicode_conformance_example();
    test_ill_formed_classes();
    test_empty_and_encode_limits();
    test_exhaustive_round_trip();
    test_fuzz_never_overreads_and_always_progresses();
    std::cout << "All UTF-8 tests passed successfully.\n";
    return 0;
}
