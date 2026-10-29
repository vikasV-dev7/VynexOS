# Inter (embedded UI font)

Static TrueType files of the **Inter** typeface, embedded into the VynexOS executable for UI text.

| | |
|---|---|
| Typeface | Inter 4.1, by Rasmus Andersson and The Inter Project Authors |
| Upstream | https://github.com/rsms/inter |
| Release | `v4.1`, published 2024-11-16 |
| Archive | `https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip` (33,707,794 bytes) |
| Archive SHA-256 | `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e` |
| Licence | **SIL Open Font License, Version 1.1** (`LICENSE.txt`, unmodified) |
| Copyright | Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter) |
| Modifications | **None.** The files are extracted unmodified and are **not subset, renamed or otherwise altered.** |

## Files

Extracted from the archive path `extras/ttf/` (static instances, TrueType outlines):

| File | Weight | Bytes | SHA-256 |
|---|---|---|---|
| `Inter-Regular.ttf` | 400 | 411,640 | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
| `Inter-Medium.ttf` | 500 | 417,300 | `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` |
| `Inter-SemiBold.ttf` | 600 | 419,744 | `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` |
| `LICENSE.txt` | - | 4,380 | `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a` (archive path `LICENSE.txt`) |

Each font also carries the OFL notice in its own `name` table (checked when vendoring).

## Licence obligations (SIL OFL 1.1) and how they are met

* **Bundling in software is permitted**, including commercially; the font itself must not be sold on its own.
* **The copyright notice and licence text must accompany the font.** `LICENSE.txt` is kept here unmodified and is
  also listed in `THIRD_PARTY_NOTICES.md` at the repository root.
* **Reserved Font Name.** Upstream's README declares: `"Inter" is a Reserved Font Name by Rasmus Andersson`
  (https://github.com/rsms/inter/blob/master/README.md; `LICENSE.txt` itself names none). Under the OFL a
  *Modified Version* may not use a Reserved Font Name, and removing glyphs (subsetting) counts as modification.
  **Therefore these files must stay unmodified and unsubset.** Do not "optimise" them with a subsetter, and do not
  rename them to something that implies they are the upstream release if they are ever changed.

## Why these three files

They back the typography tokens in `include/vynexos/desktop/theme.hpp`: Regular (body, caption), Medium
(emphasised body) and SemiBold (titles, headings, display). The variable fonts (`InterVariable*.ttf`) in the same
archive are deliberately **not** used: the rasterizer cannot select weights from a variable font.

## Integrity

`cmake/verify_third_party.cmake` fails the configure step if any file here differs from the hashes above.
`.gitattributes` marks this directory as non-text so Git never rewrites the bytes.
