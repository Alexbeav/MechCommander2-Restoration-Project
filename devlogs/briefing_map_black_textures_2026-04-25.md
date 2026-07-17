# Mission briefing map — mostly black

**Started:** 2026-04-25. Status: root cause identified, fix implemented, and
the affected briefing screen runtime-verified; broader regression smoke checks
remain.

Promoted from `devlogs/followups/briefing-map-black-textures.md` (now
deleted — superseded by this devlog).

## Symptom

Mission briefing map renders mostly black. Water and buildings are
pure black; only some darker terrain patches (green/brown) are
visible. Retail reference shows water (blue), buildings (light
grey), and terrain in natural colors — all on the same map.

The red selection marker and its "1" label still render correctly on
top, so the overlay sprite/text path is healthy. The problem is
localized to the map texture itself.

## Original hypotheses, ranked (superseded)

1. **Color-key leak with bilinear filtering.** MC2 uses a colorkey
   for transparency (commonly magenta `0xFF00FF`; some assets use
   others). If the map asset's key is close to the blue water or
   light-grey building colors, and bilinear filtering samples
   *before* the colorkey comparison, edge pixels sampling between
   water and a key-adjacent color match the key and vanish. Classic
   bilinear + colorkey footgun.
2. **Palette / indexed texture.** If the map is 8-bit palettized and
   the palette chunk isn't loaded (wrong offset, missing chunk-ID
   handling), most indices land on black and only a few map to
   visible colors.
3. **Channel / format mismatch.** BGR vs RGB swap, or DXT1 (1-bit
   alpha, zero tolerance) upload path when the loader expected
   uncompressed.

## Investigation result (2026-07-16)

The public source-data reference asset is internally healthy. Packet 3 from
`missions/mc2_02.pak` in
[`alariq/mc2srcdata`](https://github.com/alariq/mc2srcdata) is an uncompressed
128x128, 32-bit TGA whose RGB channels contain the expected blue water, grey
buildings, and natural terrain colors. Its SHA-256 is
`ac48ff458932eed627d4aa79f4744896510da326bc27b8fb46422809e530d8e0`.
Its alpha plane is a legacy binary mask: water, buildings, and other large
regions have alpha zero. Compositing that alpha over black reproduces the
reported black-region pattern.

`MissionBriefingScreen::getMissionTGA()` correctly passes the thumbnail to
`textureFromMemory()` as `gos_Texture_Solid`. That path creates an empty
texture, locks it, copies the BGRA pixels, and unlocks it. Unlike file-backed
and encoded-memory textures, the lock/unlock path bypasses
`convertIfNecessary()` / `makeKindaSolid()`, so the OpenGL texture retained
the asset's zero alpha. `aObject::render()` then uses
`gos_Alpha_AlphaInvAlpha`, causing those regions to reveal the black
destination behind the quad.

The fix belongs in `gosTexture::Unlock()`: while converting the locked BGRA
buffer to the renderer's RGBA layout, force alpha to 255 only when
`format_ == gos_Texture_Solid`. Keyed and alpha textures retain their source
alpha. This restores the existing solid-texture contract consistently across
all creation paths without special-casing the briefing screen or altering
source data.

## Validation

- The affected `gameos` library target compiles and links successfully.
- The full native Linux executable links successfully. Local build-only
  compatibility settings were used for pre-existing portability warnings and
  direct Windows-header includes; no unrelated source changes are part of this
  branch.
- The source-data TGA contains 8,668 pixels with alpha 0 and 7,716 with
  alpha 255. Modeling the patched BGRA-to-RGBA conversion makes all 16,384
  pixels opaque with no RGB changes.
- Keyed and alpha formats do not take the new branch and retain source alpha.

### Runtime result (2026-07-16)

A native Linux validation build was run against the packaged game data. The
`mc2_02` mission briefing rendered the full mission-map thumbnail through the
real OpenGL/UI path. Water remained blue, buildings and terrain retained their
source colors, and the red objective marker rendered over the map normally.
The objective-building model below the map was also textured. No black map
regions or black texture quads were visible.

The validated executable's SHA-256 was
`6ac21f0607f2b0cd6491ab74d64409de9afb58d1d973425df32df5135b7d9212`.

## Remaining regression checks

The primary affected screen is verified. Two broader smoke checks are still
recommended for other callers of the same texture path:

1. Enter a mission and check ordinary terrain and cement/colormap regions for
   obvious alpha or color regressions.
2. Open the multiplayer stats screen and check its logo textures.

The additional checks cover other callers that cache raw memory as
`gos_Texture_Solid` through the same empty-texture lock/unlock path.

## Scope boundary

- Priority: higher than `mission_select_stray_lines_2026-04-25.md`
  (this one affects gameplay readability; that one is cosmetic).
- Don't conflate with the mission-select stray-lines bug. Different
  class entirely (texture/sampler vs geometry/coord).
- Don't bundle with any other branch.
