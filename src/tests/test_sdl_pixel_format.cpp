// Regression test for the framebuffer -> SDL texture channel order.
//
// The compositor's framebuffer stores pixels as the bytes R, G, B, A. The SDL backend uploads it into a texture
// of kFramebufferTextureFormat. If that format were a packed-word format with the wrong byte order (the original
// SDL_PIXELFORMAT_ARGB8888), red and blue would be swapped on screen - e.g. the #22C7DC cyan accent would appear
// as yellow-orange. These tests pin the contract without needing a window or a GPU.
#define SDL_MAIN_HANDLED
#include "test_check.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include "../desktop/sdl2_pixel_format.hpp"

using vynexos::desktop::kFramebufferTextureFormat;

namespace {

struct Rgba {
    uint8_t r, g, b, a;
};

// Interprets 4 bytes in memory through an SDL pixel format and returns the channels SDL reports.
Rgba interpret_bytes(Uint32 format, const uint8_t (&bytes)[4]) {
    SDL_PixelFormat* fmt = SDL_AllocFormat(format);
    CHECK(fmt != nullptr);
    Uint32 word = 0;
    std::memcpy(&word, bytes, 4); // memory order, exactly as a texture upload would see it
    Rgba out{};
    SDL_GetRGBA(word, fmt, &out.r, &out.g, &out.b, &out.a);
    SDL_FreeFormat(fmt);
    return out;
}

void test_format_reads_bytes_as_rgba() {
    // Four distinct values so any permutation of channels is detected.
    const uint8_t bytes[4] = {0x11, 0x22, 0x33, 0x44}; // R, G, B, A in memory
    const Rgba c = interpret_bytes(kFramebufferTextureFormat, bytes);
    CHECK(c.r == 0x11);
    CHECK(c.g == 0x22);
    CHECK(c.b == 0x33);
    CHECK(c.a == 0x44);
    std::cout << "test_format_reads_bytes_as_rgba passed\n";
}

void test_old_format_was_wrong() {
    // Negative control: the format we used to upload with swaps red and blue. If this ever stops being true the
    // test above no longer proves anything, so it is asserted explicitly.
    const uint8_t bytes[4] = {0x11, 0x22, 0x33, 0x44};
    const Rgba old = interpret_bytes(SDL_PIXELFORMAT_ARGB8888, bytes);
    CHECK(old.r == 0x33 && old.b == 0x11); // red and blue exchanged
    CHECK(kFramebufferTextureFormat != SDL_PIXELFORMAT_ARGB8888);
    std::cout << "test_old_format_was_wrong passed\n";
}

void test_texture_upload_round_trip() {
    // Real SDL pipeline without a window: upload the framebuffer bytes into a texture of the backend's format and
    // render it onto a software surface, then read the pixels back as R,G,B,A bytes.
    constexpr int kW = 5;
    const uint8_t input[kW][4] = {
        {255, 0, 0, 255},     // pure red
        {0, 255, 0, 255},     // pure green
        {0, 0, 255, 255},     // pure blue
        {0x22, 0xC7, 0xDC, 255}, // the VynexOS cyan accent
        {10, 20, 30, 255},    // distinct, low channel values
    };

    auto round_trip = [&](Uint32 texture_format) {
        SDL_Surface* target = SDL_CreateRGBSurfaceWithFormat(0, kW, 1, 32, SDL_PIXELFORMAT_RGBA32);
        CHECK(target != nullptr);
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target);
        CHECK(renderer != nullptr);
        SDL_Texture* texture = SDL_CreateTexture(renderer, texture_format, SDL_TEXTUREACCESS_STREAMING, kW, 1);
        CHECK(texture != nullptr);

        const int updated = SDL_UpdateTexture(texture, nullptr, input, kW * 4);
        CHECK(updated == 0);
        const int cleared = SDL_RenderClear(renderer);
        CHECK(cleared == 0);
        const int copied = SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        CHECK(copied == 0);

        uint8_t output[kW][4] = {};
        const int read_back = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, output, kW * 4);
        CHECK(read_back == 0);
        bool identical = true;
        for (int x = 0; x < kW; ++x) {
            if (std::memcmp(input[x], output[x], 4) != 0) {
                identical = false;
                std::cout << "  [format " << SDL_GetPixelFormatName(texture_format) << "] pixel " << x << ": in (" << int(input[x][0]) << ","
                          << int(input[x][1]) << "," << int(input[x][2]) << "," << int(input[x][3]) << ") out (" << int(output[x][0]) << ","
                          << int(output[x][1]) << "," << int(output[x][2]) << "," << int(output[x][3]) << ")\n";
            }
        }
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(target);
        return identical;
    };

    // The backend's format reproduces every pixel exactly...
    const bool new_format_exact = round_trip(kFramebufferTextureFormat);
    CHECK(new_format_exact);
    // ...and the format it used to upload with provably does not (red and blue exchanged), so this test can fail.
    std::cout << "  (expected mismatch output for the old format follows)\n";
    const bool old_format_exact = round_trip(SDL_PIXELFORMAT_ARGB8888);
    CHECK(!old_format_exact);
    std::cout << "test_texture_upload_round_trip passed\n";
}

} // namespace

int main() {
    std::cout << std::unitbuf;
    std::cout << "Running SDL Pixel Format Tests...\n";
    test_format_reads_bytes_as_rgba();
    test_old_format_was_wrong();
    test_texture_upload_round_trip();
    std::cout << "All SDL pixel format tests passed successfully.\n";
    return 0;
}
