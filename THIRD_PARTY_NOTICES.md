# Third-party notices

VynexOS bundles or builds against the components below. Each keeps its own licence; the full texts and exact
provenance are in the listed locations.

## Bundled in the repository

### stb_truetype.h (v1.26)
* Author: Sean Barrett / RAD Game Tools
* Licence: MIT **or** Public Domain (Unlicense), at the user's choice. Copyright (c) 2017 Sean Barrett.
* Text and provenance: `third_party/stb/LICENSE`, `third_party/stb/README.md`
* Used for: rasterizing the embedded UI fonts into the text glyph atlas.

### Inter (4.1: Regular, Medium, SemiBold)
* Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)
* Licence: SIL Open Font License, Version 1.1. "Inter" is a Reserved Font Name; the font files are bundled
  unmodified and unsubset, as the licence requires.
* Text and provenance: `assets/fonts/inter/LICENSE.txt`, `assets/fonts/inter/README.md`
* Used for: all UI text, embedded into the executable.

### font8x8 (basic Latin bitmap font)
* Public domain; based on public-domain IBM VGA fonts (Marcel Sondaar), packaged by Daniel Hepper.
* Location: `src/desktop/font8x8_basic.h` (header carries its own notice).
* Used for: fallback text rendering if the embedded font cannot be initialised.

## Fetched at build time (not stored in the repository)

### SDL2 (release-2.30.9)
* Licence: zlib. Fetched by CMake `FetchContent` from https://github.com/libsdl-org/SDL.
* Used for: window, rendering surface and input on the hosted desktop.
