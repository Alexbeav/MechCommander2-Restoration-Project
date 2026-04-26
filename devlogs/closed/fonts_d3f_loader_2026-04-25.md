# Fonts — port `.d3f` loader (CLOSED)

**Started:** 2026-04-25. **Closed:** 2026-04-26. **Status:** loader
landed and rendering retail fonts in production across Main Menu,
Mission Selection, Mission Briefing, Mech Bay, Pilot Ready,
in-mission HUD. Vertical metrics solved by ASCII-restricted
alpha-scan calibration + `.glyph` sidecar bridge for
`font_line_skip_` and `max_advance_`. Centered button widgets
rewired to `gos_TextVisualBounds()` (per-glyph ink bounds,
runtime-only on `gosGlyphInfo`). Newline-glyph rendering guard
added. Renderer-contract bug found and fixed during review. Residual
`+1` caller bias at rewired centering sites dropped after
verification on commit `a9e7b46`.

Final commits on `fonts` branch:
- `0a1ed6b` — D3F loader + ASCII-restricted calibration + sidecar
- `3a0bb12` — visual-bounds API + 3 centering rewires
- `a9e7b46` — drop residual +1 bias

Remaining font work tracked in
[`devlogs/followups/font-tweaks-residual.md`](../followups/font-tweaks-residual.md):
top-aligned screen headers sit too high, stroke-weight upscale
artifact (deferred), mech-slot label width spillage (UI layout work
in `mcl_gn_deploymentteams.fit`), other in-game fonts may need
tweaking once they get attention.

Promoted from `devlogs/followups/font-rendering-regression.md` (since
deleted).

## Motivation

Retail ships `.d3f` fonts; the GL port previously only knew
`.bmp`+`.glyph` produced by an off-repo converter. That converter
dropped `cA`/`cC` per-character spacing and **normalized vertical
metrics** (e.g. agencyfb14.glyph and agencyfb17.glyph both report
ascent=13/line_skip=14 despite ~2× different D3F em-box heights).
Result: kerning and per-font size intent were both lost.

## Why isn't this just "use the original source"?

The MS shared-source drop (`MC2_Source_Code/`) only contains GameOS
headers — no `.cpp` implementations. GameOS was originally delivered
as binary `.lib` files (`GameOS/Lib/{Debug,Profile,Release}/GameOS.lib`),
deleted in alariq commit `5a8e07f` (2016) when the GL port began.
Microsoft kept `Font3D.cpp` proprietary; we have only `Font3D.hpp`.

Writing the loader was new code, but the header specifies the binary
layout, so it's a bounded struct-parsing job, not reverse engineering.

## Format — corrected from disk inspection

`Font3D.hpp` is incomplete: it documents the v1 signature only and
treats the atlas as if it lived in a separate file. Both wrong.

### Signatures (empirical)

```
v1  "D3DF"   bytes 44 33 44 46   little-endian DWORD = 0x46443344
v4  "D3F4"   bytes 44 33 46 34   little-endian DWORD = 0x34463344
```

The shipped header at `GameOS/include/font3d.hpp:18,37` only documents
the v1 value. The v4 constant is empirical — confirmed across
`AgencyFB.d3f`, `agencyfb14.d3f`, `arial8.d3f`, `Impact20.d3f` (v4)
and `abbey11.d3f` (v1).

### Atlas is embedded in the `.d3f` file

For v4, `iTextureCount` `D3DFontTexture` blobs follow the header
inline. **No companion `.tga` file** — the 4 `.tga` files in
`full_game/assets/graphics/` are unrelated to D3F.

`dwSize` is the atlas **side length** (atlases are square 8-bit alpha).
Sizes observed: 256 (most v4 fonts) and 128 (`arialnarrow6.d3f`).
Loader trusts per-blob `dwSize` rather than hardcoding 256.

### v4 (`D3DFontData1`, `#pragma pack(1)`)

```cpp
struct D3DFontData1 {
    DWORD  dwSig;            // 0x34463344 ("D3F4")
    char   szFaceName[64];
    int32  iSize;            // point size
    uint8  bItalic;          // 1 byte under pack(1)
    int32  iWeight;
    int32  iTextureCount;    // count of inline atlas blobs
    DWORD  dwFontHeight;     // glyph band height in atlas (em-box)
    BYTE   bTexture[256];    // per-char: which atlas
    BYTE   bX[256];          // per-char: X in atlas
    BYTE   bY[256];          // per-char: Y in atlas
    BYTE   bW[256];          // per-char: width in atlas
    int8   cA[256];          // signed pen pre-spacing
    int8   cC[256];          // signed pen post-spacing
};
// followed by iTextureCount * D3DFontTexture blobs:
//   DWORD dwSize;            // atlas side length (square)
//   BYTE  pixels[dwSize*dwSize];   // 8-bit alpha
```

Pen advance = `cA[c] + bW[c] + cC[c]`.

### v1 (`D3DFontData`)

```cpp
struct D3DFontData {
    DWORD  dwSig;           // 0x46443344 ("D3DF")
    DWORD  dwWidth;         // atlas width (pixels)
    DWORD  dwFontHeight;    // glyph band height
    DWORD  dwHeight;        // atlas height
    DWORD  dwX[256];
    DWORD  dwY[256];
    DWORD  dwWidths[256];
    int32  nA[256];         // pre-spacing (full int)
    int32  nC[256];         // post-spacing (full int)
    BYTE   bPixels[];       // dwWidth * dwHeight bytes, 8-bit alpha
};
```

13 of 28 retail `.d3f` files are v1 — v1 support is **mandatory**.
v1 atlas is inlined directly without the `dwSize` prefix.

### Field-by-field reads

Each field read with explicit `fread`, not blob-reading the struct.
Avoids `#pragma pack(1)` and `sizeof(bool)` portability traps across
MSVC / g++.

## Renderer contract bug found during integration (H1 — fixed)

The pre-existing renderer at `gameos_graphics.cpp:2080` computed:

```
char_off_x = gm.minx;
char_w     = gm.maxx - gm.minx;
iu0 = gm.u + char_off_x;     // ← couples atlas sample to screen bearing
iu1 = iu0 + char_w;
```

Under the legacy `.glyph` contract this was correct: `u` was a
**cell origin**, and `minx` was an in-cell offset that shifted both
screen quad position AND atlas sample by the same amount.

The naive D3F mapping (`u = bX[c]`, `minx = cA[c]`) inherits that
arithmetic with non-cell semantics: `iu0 = bX + cA` instead of `bX`.
Off by `cA` pixels. With densely-packed atlases (no inter-glyph
padding), every char with `cA != 0` samples 1+ column of the adjacent
glyph in the atlas. **Cross-talk visible as ghost edges.**

`arialnarrow8` mostly survived the bug (cA=0 for A/M/i/space, only `.`
had cA=1). AgencyFB11/14/17 had widespread `cA=1` and showed visible
corruption.

### Fix — decouple the renderer (Option 1)

`gosGlyphMetrics` post-load contract changed:

| Field | Old (legacy-coupled) | New (decoupled) |
|-------|----------------------|-----------------|
| `u/v` | cell origin | actual atlas top-left of glyph pixels |
| `minx/maxx/miny/maxy` | in-cell offset (also shifted atlas) | screen-space pixel rect (signed bearing) |

Renderer change (`gameos_graphics.cpp:2080`, ~2 lines):
```c
uint32_t iu0 = gm.u;          // was: gm.u + char_off_x
uint32_t iv0 = gm.v;          // was: gm.v + char_off_y
```
`char_off_x` / `char_off_y` still drive screen quad position; just
not the atlas read.

Loader fixups:
- `gos_load_d3f` (parse_v4 / parse_v1): no change. `g.u = bX[c]`,
  `g.v = bY[c]` was already correct under the new contract.
- `gos_load_glyphs`: pre-fold `g.u += g.minx` and
  `g.v += (font_ascent - g.maxy)` after blob fread, so the legacy
  `.bmp+.glyph` path renders pixel-identical under the new contract.
  Underflow guard SPEWs and clamps to 0 — keeps community-font
  fallback alive while making malformed metrics visible.

On-disk format unchanged for both `.glyph` and `.d3f`. `getCharUV()`
has no other callers (verified) so the contract change is safe.

## Texture-state hygiene (landed)

Three issues fixed in the text-draw block (`gameos_graphics.cpp:2105+`):

1. **`Filter` was leaked.** Only `Texture` was saved/restored. Now
   saves `Filter` too.
2. **`TextureAddress` defaulted to wrap.** `gameos_graphics.cpp:1416`
   sets `gos_TextureWrap` as the default; the text path didn't override.
   Atlas-edge glyphs sampled across the seam under wrap mode.
3. **Now forced to `gos_TextureClamp` during text draw**, restored
   after.

All three states (Texture, Filter, TextureAddress) save before the
override and restore after `text_->draw(mat)`.

## Texture format — RGBA expansion baseline

`gos_text.frag:19-20` samples `texture(tex1, ...).xxxx`. The empty
texture path at `gameos_graphics.cpp:915` hardcodes `TF_RGBA8`, and
`gos_TextureFormat` (`gameos.hpp:2716`) has no `R8` entry exposed.
`TF_R8` exists in `utils/render_constants.h` but is private to the GL
layer.

Crucially, `gos_Texture_Alpha` is used outside fonts —
`code/mc2movie.cpp:176` relies on it resolving to RGBA8 for video
frame uploads. Redefining it would regress FMV.

Loader expands the 8-bit alpha atlas to RGBA8 at load (fans alpha into
all four channels), allocates via
`gos_NewEmptyTexture(gos_Texture_Alpha, name, RECT_TEX(w,h), 0)`, and
uploads via `glTexSubImage2D` against `gos_GetTextureGLId(handle)` —
same one-shot pattern `MC2Movie` uses per frame, just done once at
font load. Zero plumbing changes.

`TF_R8` font-local spike is a deferred VRAM optimization, not a
prerequisite.

## Vertical metrics — current approach

D3F's `dwFontHeight` is the **em-box height** (cap + descender +
leading allowance for accented glyphs). Used directly as
`font_ascent_` / `font_line_skip_`, multi-line text packs at ~2×
retail's density. Two-stage trimming brings it back to retail
geometry without hardcoded magic numbers.

### Stage 1 — `calibrate_vertical` over printable ASCII (0x20-0x7E)

In `gos_font.cpp`, `calibrate_vertical` alpha-scans visible glyphs in
the atlas, finds the tightest opaque-row band, and uses it as
`visible_height` (the rendered band per glyph) while shifting `g.v` by
`top_trim` so the band aligns with the glyph quad. Per-glyph
`g.maxy = visible_height`, `g.miny = 0` — D3F doesn't encode a
baseline, so all glyphs share the trimmed band's vertical extent.
`gi.font_ascent_` is also set to `visible_height` so the renderer's
`char_off_y = font_ascent − maxy = 0` anchors each glyph at line top.

**Scan restricted to printable ASCII (0x20-0x7E).** Extended-ASCII
accented capitals (Ä Ö Ü at 0xC4/0xD6/0xDC etc.) reach 3-5 px higher
than ordinary caps in the AgencyFB atlases. Letting them drive
`global_top` adds permanent blank headroom to every line of UI text
and visibly pushes ASCII content low (most obvious on the
mission-selection red header — non-centered, so the bias was a pure
calibration symptom, not centering math). Trade-off: accents on
extended chars may render slightly clipped at the top, but those
chars are vanishingly rare in MC2's UI strings.

### Stage 2 — `.glyph` sidecar bridge for `font_line_skip_` + `max_advance_`

In `gosFont::load`, after `gos_load_d3f` succeeds, probe for
`dir/fname.glyph`. If present, read its first 20 bytes (legacy header:
u32 num_glyphs, start_glyph, max_advance, ascent, line_skip) and
adopt only `font_line_skip_` and `max_advance_`. The `.glyph` sidecar
already encodes retail's per-font line spacing, so we get the right
values without a hardcoded override table or a derived heuristic.

**Do not adopt `font_ascent_` from the sidecar.** The calibrated band
height (Stage 1) must stay consistent with the per-glyph `maxy`/`miny`
set during calibration — otherwise the renderer's
`char_off_y = font_ascent − maxy` goes off-zero and pushes text up
out of frame (legacy ascent < calibrated band) or back down
(legacy ascent > calibrated band).

Fonts without a sidecar fall back to `font_line_skip_ = visible_height
+ 1` from `calibrate_vertical`. Community fonts shipped with only a
`.d3f` get reasonable defaults; community fonts shipped as
`.bmp+.glyph` go through the legacy path with no sidecar bridge needed.

### Dead ends (documented so they aren't re-explored)

- **`.glyph`-derived ascent taken whole-cloth** drifts text up out of
  frames. Legacy ascent (~13 for agencyfb14) is smaller than the
  calibrated band (~17 with descenders), making `char_off_y` go
  negative when per-glyph `g.maxy` stays at the calibrated value.
- **Per-glyph re-anchor** (set `maxy = legacy_ascent`,
  `miny = legacy_ascent − calibrated_band`) is a no-op for the top
  edge — `char_off_y = font_ascent − maxy = 0` regardless of the
  values you pick, as long as `font_ascent = maxy`.
- **Alpha-scan over all 256 glyph slots** captures rare accented
  capitals' headroom and biases everything low. Restricting the scan
  to printable ASCII eliminates the bias.
- **Hardcoded per-font line_skip override table**
  (`agencyfb11→11`, `agencyfb14→14`, etc.) worked for the fonts in
  the table but was a compatibility list, not a derivation. Sidecar
  bridge supersedes it — same values, no magic constants in the loader.

## Newline character rendering

`findTextBreak` (`gameos_graphics.cpp:1909`) consumes the trailing
`\n` in its returned char count to drive line advancement via
`y += font_height` in the caller. The legacy `.glyph` files had a
zero-width entry at index 10 (LF) so the per-character render loop
drew nothing visible for the consumed `\n`. D3F atlases ship a
placeholder/tofu glyph at index 10 (and other control char slots)
that would render as a square at end-of-line and again on `\n\n`
blank-line breaks (briefing body text was the most visible offender).

**Fix:** the per-character loop in `gosRenderer::drawText`
(`gameos_graphics.cpp:2076+`) skips `c < 0x20` before
`getGlyphMetrics`/`addCharacter`. Line-advance behavior is unchanged
because `findTextBreak` still reports the `\n` in `num_chars`.

## Loader dispatch

`gosFont::load` (`gameos_graphics.cpp:2195`):

1. Resolve `dir/fname.d3f`. If present and `gos_load_d3f` returns true
   AND `gos_NewEmptyTexture` returns non-zero, take the d3f path.
2. Else fall back to existing `dir/fname.bmp` + `dir/fname.glyph`.

Callers pass paths like `"assets/graphics/arial8.tga"`
(`code/logmain.cpp:459`, `gui/afont.cpp:35`); `_splitpath` strips the
extension, so the basename probe preserves every callsite.

`.bmp`+`.glyph` fallback stays **permanent**, not transitional.
Useful for community fonts without D3F tooling.

## Code touch points

- `GameOS/gameos/gos_font.cpp` — `gos_load_d3f` (parse v1/v4 + atlas
  extract + ASCII-restricted alpha-scan calibration in
  `calibrate_vertical`, plus per-glyph ink-bounds population in a
  second pass over all 256 slots). `gos_load_glyphs` adds pre-fold
  step for legacy `.glyph` compatibility under the new renderer
  contract.
- `GameOS/gameos/gos_font.h` — declares `gos_load_d3f` + `gosD3FAtlas`;
  `gosGlyphInfo` carries runtime-only `ink_top_/ink_bot_/ink_valid_`
  arrays for the visual-bounds API.
- `GameOS/gameos/gameos_graphics.cpp` — `drawText` per-character loop
  skips control chars (`c < 0x20`); renderer-contract decouple
  (`gm.u`/`gm.v` used directly without `+ char_off_x/y`); text-draw
  render-state save/restore (Texture, Filter, TextureAddress) with
  forced clamp; `gosFont::load` with d3f-first probe and `.glyph`
  sidecar bridge for `font_line_skip_` + `max_advance_`;
  `gos_TextVisualBounds()` + `gosFont::getGlyphInfo()` accessor;
  destructor frees ink arrays.
- `GameOS/include/gameos.hpp` — declares `gos_TextVisualBounds()`.
- `gui/abutton.cpp:176` — `aButton::render` rewired to
  `gos_TextVisualBounds()` for vertical centering.
- `code/controlgui.cpp:1511` — `ControlButton::render` rewired
  similarly.
- `gui/alistbox.cpp:1306` — `aTextListItem::render` centering branch
  rewired (top-aligned branch left alone).

## Scope deliberately out

- **Stroke-weight tuning.** Current text reads slightly bolder than
  retail; partly an upscale-factor artifact (nearest filtering at
  2×+ display upscale produces harder edges than retail's
  near-1:1). Not a font-loader bug. Defer.
- **In-mission HUD parity.** User confirmed retail in-mission HUD
  looks bad at >800×600 too — known retail problem, vanilla+
  improvement, not parity work.
- **Asset cleanup.** 12 redundant `.bmp`+`.glyph` files in
  `full_game/assets/graphics/` could be removed once the d3f path is
  fully validated. Asset-cleanup commit, not code work.
- **`TF_R8` font-local optimization.** VRAM halving for atlases.
  Low priority.
- **Multi-atlas v4 fonts** (`iTextureCount > 1`). Not observed in
  local corpus; loader rejects with SPEW.

## Status snapshot

- [x] v1 + v4 parser
- [x] Embedded atlas extraction
- [x] gosGlyphMetrics post-load mapping
- [x] Renderer contract decouple (H1 fix)
- [x] `.glyph` pre-fold for legacy compatibility
- [x] Texture-state save/restore + clamp
- [x] ASCII-restricted alpha-scan calibration
- [x] Newline rendering guard
- [x] `.glyph` sidecar bridge for `font_line_skip_` + `max_advance_`
- [x] Per-glyph ink bounds + `gos_TextVisualBounds()` API
- [x] Centered button widgets rewired (aButton, ControlButton,
      aTextListItem centering branch)
- [x] Visual validation across Main Menu, Mission Selection, Mission
      Briefing, Mech Bay, Pilot Ready, in-mission HUD

## Visual centering — `gos_TextVisualBounds()` API (commit 2)

`gos_TextStringLength()` returns `lines * font_line_skip_` for height,
which overstates the visible glyph-bbox by the leading included in
line_skip. Centered widgets (`aButton::render`, `ControlButton::render`,
`aTextListItem::render` centering branch) compute
`(rect.top + rect.bot)/2 − height/2` — biasing ALL CAPS text low by
`(line_skip − cap_height)/2 ≈ 2-4 px`. Plus the per-glyph quad
includes empty rows in the descender slot for caps-only strings, so
returning a smaller "visual height" alone wouldn't fix it (would just
shift the quad top down).

### Storage

`gosGlyphInfo` got three runtime-only pointer fields (NOT in
`gosGlyphMetrics` — would break the legacy `.glyph` blob-read at
`gos_load_glyphs`):

- `int8_t* ink_top_` — first inked atlas row, signed offset relative
  to rendered quad top. Negative for extended glyphs that sit above
  the ASCII-trimmed band.
- `int8_t* ink_bot_` — last inked atlas row, same reference frame.
- `uint8_t* ink_valid_` — separate validity flag (can't sentinel on
  `top==bot==0` since a real glyph may legitimately span row 0).

Populated in `calibrate_vertical` by a second pass over all 256
glyphs (extended-ASCII glyphs need bounds too, even though they
don't drive the band metrics). Allocated only on D3F path; legacy
`.glyph` fonts leave them NULL. Destructor frees alongside `glyphs_`.

### API

```cpp
void __stdcall gos_TextVisualBounds(
    DWORD* Width, int* Top, int* Bottom,
    const char* Message, ...);
```

Width matches `gos_TextStringLength()` (max line width across `\n`).
Top/Bottom signed. Multi-line:
`Bottom = (lines − 1) * line_skip + last_line_bot`. Whitespace-only
returns `(0, 0)`. NULL ink arrays (legacy fonts) fall back to
`Top=0, Bottom=lines*line_skip` so centering math degrades to the
existing line-skip behavior — no improvement, no regression.

### Caller rewires

- `gui/abutton.cpp:176` — `aButton::render` button labels. Existing
  `+1` bias preserved.
- `code/controlgui.cpp:1511` — `ControlButton::render` HUD buttons.
  Existing `+1` bias preserved. Added explicit
  `gos_TextSetAttributes` so the bounds query sees the same font as
  the subsequent `aFont::render`.
- `gui/alistbox.cpp:1306` — `aTextListItem::render` centering branch
  ONLY. The `forceToTop` / top-aligned else branch at `:1309`
  untouched. No `+1` (matches original float-math style).

### Untouched

`gos_TextStringLength()`, `aFont::height()` (both forms), and all
~14 other `gos_TextStringLength` callers (afont word-wrap, controlgui
help text, debugging overlay, float-help, width-only paths) keep
line-skip semantics. ~12 widget-stride consumers via cached
`aFont::height()` (chatwindow, alistbox sizing, pilotreview,
pilotready, asystem) untouched.

### Known residuals (next passes)

- **`+1` caller bias may now be net-low.** The `+1` preserved in
  `aButton`/`ControlButton` was a compensation for the original
  line-skip-based centering bug. Now that visual-bounds math centers
  on actual ink, the `+1` shifts everything 1 px below true center.
  Visible across most centered labels in the 2026-04-26 verification
  set. Drop the `+1` (separately, after re-verifying every rewired
  site looks correct without it).
- **Top-aligned screen headers sit too high.** `MECH BAY`,
  `MISSION SELECTION`, `MISSION BRIEFING`, `PILOT READY AREA`,
  `LOAD GAME`, etc. — top-left corner labels — render via a
  top-aligned path (not aButton/ControlButton/aTextListItem
  centering), so the visual-bounds rewire doesn't reach them. Need
  to identify the render path and add a few px of top padding.
- **Stroke-weight upscale artifact.** D3F atlases are pure 1-bit
  (alpha is 0 or 255, no AA). At 2×+ display upscale with nearest
  filtering, every 1-px stroke becomes 2-3 display pixels, reading
  as a heavier weight than retail's near-1:1 render. Confirmed by
  inspecting `arialnarrow8.d3f` (header `weight=400`, regular)
  against the legacy `.bmp` atlas (also 1-bit, same character pixel
  density). Not a loader bug.
- **Mech-slot label width spillage.** Legacy `.glyph` for
  `arialnarrow8` has zero-width entries for lowercase letters (the
  legacy `.bmp` atlas only stores uppercase + digits). Retail-via-D3F
  shows full lowercase, so labels like "BUSHWACKER\nLongshot" now
  render with a wider visible footprint than the legacy port did.
  Correct rendering behavior; the real fix is widening the slot box
  in `mcl_gn_deploymentteams.fit` — UI layout work.
