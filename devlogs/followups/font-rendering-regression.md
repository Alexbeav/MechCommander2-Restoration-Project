# Font rendering quality regression

Discovered 2026-04-24. Status: investigated, fix path identified, not
yet built.

The current build renders the in-game UI font noticeably worse than
retail — hard to read at normal resolutions.

## Root cause

Our build hardcodes `.bmp` + `.glyph` as the font format
(`GameOS/gameos/gameos_graphics.cpp:2190-2191`, consumed by
`gosFont::load`), but retail ships `.d3f` + `.tga`. The
`full_game/assets/graphics/*.{bmp,glyph}` files that make the game
boot are a pre-converted set inherited from an upstream fork; they
look worse than the retail `.tga` glyph atlases sitting alongside
them.

## Investigation starting points

- `GameOS/gameos/gos_font.cpp:10-40` — `gos_load_glyphs` reads the
  `.glyph` sidecar (num_glyphs, start_glyph, max_advance, font_ascent,
  font_line_skip, per-glyph metrics). That's the metrics side.
- `gosFont::load` at `gameos_graphics.cpp:2185` — pairs the `.glyph`
  metrics with a `.bmp` atlas loaded via `gosTexture`.
- Retail `.tga` + `.d3f` pair lives untouched in every retail install
  (`assets/graphics/arial8.tga`, `arial8.d3f`, etc.). The `.d3f` is
  the original bitmap-font container; see
  `MC2_Source_Code/Source/GameOS/include/Font3D.hpp` for the original
  structure.

## Likely fix (not verified)

Write a small one-shot tool that reads a retail `.tga`+`.d3f` pair and
emits a matching `.bmp`+`.glyph` pair preserving the retail pixel data
and metrics. That should give retail-identical font quality against
the current binary without touching engine code. ~50-100 lines of C++
leaning on the existing gosTexture code.

## Alternative

Patch the engine to load `.d3f`+`.tga` directly and skip the
intermediate format. More work, and risks regressing whatever
motivated the original conversion.
