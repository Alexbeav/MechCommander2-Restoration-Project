# Font rendering quality regression

Status: investigation done, approach picked, not yet built.

The current build renders the in-game UI font noticeably worse than
retail — hard to read at normal resolutions. User despises the current
fonts.

## Root cause

`GameOS/gameos/gameos_graphics.cpp:2190` hardcodes `.bmp` + `.glyph` as
the only font format the loader understands:

```cpp
const char* tex_ext = ".bmp";
const char* glyph_ext = ".glyph";
```

The `.glyph` sidecar is a custom binary format defined in
`GameOS/gameos/gos_font.cpp:10-40`. It carries `num_glyphs`,
`start_glyph`, `max_advance`, `font_ascent`, `font_line_skip`, then an
array of per-glyph metrics. Created by an off-repo one-shot converter
— the tool itself isn't in the tree.

The original retail format is `.d3f` (documented in
`MC2_Source_Code/Source/GameOS/include/Font3D.hpp`). Two versions exist:

- **v1.0** (`D3DFontData`) — embedded pixel data, per-char X/Y/widths
  and `nA`/`nC` prefix/suffix spacing.
- **v4.0+** (`D3DFontData1`) — external `.tga` atlases + per-char
  texture index, X/Y/width, plus the `cA`/`cC` spacing values.

**No `.d3f` loader exists in the GL port.** The format carries more
data than the `.glyph` sidecar preserves (specifically the `cA`/`cC`
prefix/suffix spacing, which governs kerning between characters). Any
converter that doesn't round-trip those values produces visibly worse
spacing than retail.

**Current state on disk** (`full_game/assets/graphics/`):

- 28 `.d3f` files — retail originals, ignored by the loader
- 4 `.tga` files — retail atlases for the v4.0 fonts, also ignored
- 12 `.bmp`/`.glyph` pairs — converted subset, actively loaded

So retail fonts are sitting right there, unused.

## Approach — port the `.d3f` loader (chosen)

Add a `.d3f` file reader alongside the existing `.bmp` + `.glyph`
path in `gosFont::load`. The engine loads retail fonts directly; no
offline conversion step, no quality-lossy converter to maintain.

Rough shape:

1. In `gosFont::load` (`gameos_graphics.cpp:2185`), try a `.d3f` file
   first. If found, take the v4.0 parse path (it's what retail ships);
   fall back to `.bmp`+`.glyph` if no `.d3f` exists (preserves the
   current converted fonts so nothing breaks).
2. `.d3f` v4.0 parse: read header (sig `0x46443344` = "FD3D"), face
   name, texture count, per-char arrays. For each texture index 0..N,
   load the companion `.tga` via the existing `gosTexture` path.
3. Map `.d3f`'s per-char metrics (`bW`, `cA`, `cC`, `bTexture`, `bX`,
   `bY`) onto the engine's `gosGlyphMetrics` struct — or extend the
   struct to carry `cA`/`cC` if they're currently lost. The spacing
   fields are probably what's missing from the converted fonts.
4. Test against a font that looks bad today (user despises them all;
   any of the `Impact*` or `agency*` set should work). Compare
   character spacing + glyph alpha against a retail screenshot.

~60 lines of struct parsing against `Font3D.hpp`. Similar effort to
writing the external converter, better long-term outcome.

## Rejected alternative

**Offline `.tga`+`.d3f` → `.bmp`+`.glyph` converter.** The previous
plan. Rejected because it adds a build-time step, keeps the conversion-
artifact layer, and if any future retail font becomes desirable we'd
need to re-run the tool. Porting the loader removes that whole class
of problem.
