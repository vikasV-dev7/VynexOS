#pragma once
#include <SDL.h>

namespace vynexos::desktop {

/// Pixel format of the streaming texture the compositor's framebuffer is uploaded into.
///
/// The framebuffer (and every software surface) stores each pixel as the bytes R, G, B, A in memory.
/// SDL_PIXELFORMAT_RGBA32 is SDL's byte-order alias for exactly that layout on any host: it resolves to
/// ABGR8888 on little-endian systems and RGBA8888 on big-endian ones. (The packed-word names such as ARGB8888
/// describe a 32-bit integer, so on little-endian hosts they mean a different byte order and swap red and blue.)
inline constexpr Uint32 kFramebufferTextureFormat = SDL_PIXELFORMAT_RGBA32;

} // namespace vynexos::desktop
