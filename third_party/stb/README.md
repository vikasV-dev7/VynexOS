# stb_truetype (vendored)

Single-header TrueType/OpenType rasterizer used by the VynexOS text renderer.

| | |
|---|---|
| Library | `stb_truetype.h` **v1.26** (2021-08-28), "authored from 2009-2021 by Sean Barrett / RAD Game Tools" |
| Upstream | https://github.com/nothings/stb |
| Pinned commit | `6e9f34d5429cf16790ec43c9bac3f1ee4ad1f760` (2024-07-15, last commit touching this file at the time of vendoring; a merge commit) |
| Downloaded from | `https://raw.githubusercontent.com/nothings/stb/6e9f34d5429cf16790ec43c9bac3f1ee4ad1f760/stb_truetype.h` |
| Size | 199,192 bytes |
| SHA-256 | `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab` |
| `LICENSE` SHA-256 | `bebfe904b14301657e4e5d655c811d51fd31b97c455b9cc2d8600d6bac6cff63` (2,510 bytes, same pinned commit) |
| Modifications | **None.** Both files are byte-for-byte copies of upstream. |

## Licence

The upstream `LICENSE` states: "This software is available under 2 licenses -- choose whichever you prefer."
**Alternative A - MIT License** (Copyright (c) 2017 Sean Barrett) or **Alternative B - Public Domain
(www.unlicense.org)**. The same block is at the end of `stb_truetype.h`. The unmodified `LICENSE` is kept next to
the header; the MIT alternative's notice is therefore always carried with the code.

## Security constraint

The header says: "NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES. This library does no range
checking of the offsets found in the file, meaning an attacker can use it to read arbitrary memory."

VynexOS therefore **only loads the audited fonts embedded in the executable** (see `assets/fonts/inter/`), whose
SHA-256 values are verified when the project is configured. Do not add an API that parses user-supplied or
downloaded font files with this library.

## Scope notes

* Static fonts only: the header has no handling of variable-font tables (`fvar`/`gvar`), so variable fonts such as
  `InterVariable.ttf` are not usable.
* Kerning: the `kern` table and basic OpenType GPOS pair adjustment (added in v1.19/v1.22).

## Integrity

`cmake/verify_third_party.cmake` fails the configure step if either file here differs from the hashes above.
`.gitattributes` disables newline conversion for this directory so `core.autocrlf` cannot change the bytes.
