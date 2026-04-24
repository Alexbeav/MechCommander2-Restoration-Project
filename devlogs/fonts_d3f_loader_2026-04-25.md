# Fonts — port `.d3f` loader

**Started:** 2026-04-25. Status: not coded yet; approach chosen.
Promoted from `devlogs/followups/font-rendering-regression.md` (now
deleted — superseded by this devlog).

## Motivation

User despises current font rendering. Root cause: the GL port's
loader only understands `.bmp`+`.glyph` (a custom sidecar format
produced by a lossy off-repo converter); retail ships `.d3f`+`.tga`
and those files already sit in `full_game/assets/graphics/` (28
`.d3f` + 4 `.tga`), ignored. The converter probably dropped the
`cA`/`cC` per-character spacing fields, which governs kerning —
that's a plausible source of the quality delta.

## Why isn't this just "use the original source"?

The MS shared-source drop (`MC2_Source_Code/`) only contains headers
for GameOS — no `.cpp` implementations. GameOS was originally
delivered as binary `.lib` files (`GameOS/Lib/{Debug,Profile,Release}/
GameOS.lib`), deleted in alariq commit `5a8e07f` (2016) when the GL
port began. Microsoft kept `Font3D.cpp` proprietary. We have only
`Font3D.hpp`.

So writing the loader is genuinely new code — but the header fully
specifies the binary layout, so it's a bounded struct-parsing job,
not reverse engineering.

## Format — per `MC2_Source_Code/Source/GameOS/include/Font3D.hpp`

Two versions exist. Retail ships v4.0+ (`D3DFontData1`):

```cpp
struct D3DFontData1 {
    DWORD  dwSig;           // 0x46443344 "FD3D"
    char   szFaceName[64];
    int    iSize;           // point size
    bool   bItalic;
    int    iWeight;
    int    iTextureCount;   // number of companion .tga atlases
    DWORD  dwFontHeight;
    BYTE   bTexture[256];   // per-char: which texture atlas
    BYTE   bX[256];         // per-char: X position in atlas
    BYTE   bY[256];         // per-char: Y position in atlas
    BYTE   bW[256];         // per-char: width in atlas
    signed char cA[256];    // per-char: pre-spacing (may be negative)
    signed char cC[256];    // per-char: post-spacing (may be negative)
};
```

For each texture 0..`iTextureCount-1`, load
`{fontname}{N}.tga` as a companion atlas (indexed by `bTexture[c]`).
Glyph quad for char `c` comes from `(bX[c], bY[c], bW[c], height)` in
atlas `bTexture[c]`, with advance = `cA[c] + bW[c] + cC[c]`.

There's also v1.0 (`D3DFontData`) with embedded pixels — skip unless
we find a retail font that uses it.

## Implementation plan

1. **Reader.** New function `gos_load_d3f(const char* fontFile,
   gosGlyphInfo& gi, std::vector<gosTexture*>& atlases)` in
   `GameOS/gameos/gos_font.cpp`. Parse v4.0 struct, validate
   `dwSig == 0x46443344`, populate the engine's internal per-glyph
   metrics.

2. **Metrics mapping.** Current `gosGlyphMetrics` likely doesn't
   carry `cA`/`cC` (since the converter dropped them). Extend it
   with `pre_space`/`post_space` signed-int fields and update the
   text-rendering path to apply them. If the current loader stores
   a single "advance" value, switch it to computing
   `advance = cA + width + cC` at draw time.

3. **Loader dispatch.** In `gosFont::load`
   (`gameos_graphics.cpp:2185`), try `.d3f` first. If present, take
   the new path. If not, fall back to the existing `.bmp`+`.glyph`
   path so the 12 converted fonts keep working (no asset migration
   pressure).

4. **Multi-atlas handling.** Current font code assumes one texture
   per font; `.d3f` v4.0 supports N. Per-char `bTexture[c]` selects
   which. Either draw in per-atlas batches (group chars by atlas
   before emitting quads) or assert `iTextureCount == 1` for now
   and revisit if any retail font actually uses more.

5. **Test.** Side-by-side against a converted font where the quality
   complaint is most visible. Any of `Impact*` or `agencyfb*` is
   fine. Compare character spacing + glyph alpha against retail.

## Code touch points

- `GameOS/gameos/gos_font.cpp:10-40` — existing `gos_load_glyphs`
  for `.glyph` sidecar. Add `gos_load_d3f` alongside.
- `GameOS/gameos/gameos_graphics.cpp:2185` — `gosFont::load` entry
  point. Add `.d3f` first-try.
- `GameOS/gameos/gos_font.h` — `gosGlyphInfo` / `gosGlyphMetrics`
  struct definitions. Likely need `cA`/`cC` fields.
- `MC2_Source_Code/Source/GameOS/include/Font3D.hpp` — format
  reference. Copy the struct defs into a new header in our tree
  rather than reaching into the reference checkout.

## Scope boundary

- NOT in scope: rendering engine changes beyond what's needed to
  consume new metrics. If a font still looks wrong after correct
  parsing, that's a separate investigation.
- NOT in scope: converting existing `.bmp`+`.glyph` to `.d3f`.
  They're left as fallback; retail `.d3f` wins when present.
- NOT in scope: custom font support for modders. Retail fonts
  only for now.

## Followups this spawns if shipped

- If multi-atlas fonts exist and produce texture-binding overhead:
  atlas packing pass as an optimization. Low priority; there are
  only ~2 texture switches per frame max for UI.
- Strip the 12 redundant `.bmp`+`.glyph` files from
  `full_game/assets/graphics/` if the corresponding `.d3f`+`.tga`
  pair works cleanly. Asset-cleanup commit, not a code fix.
