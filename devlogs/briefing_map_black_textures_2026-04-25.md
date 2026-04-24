# Mission briefing map — mostly black

**Started:** 2026-04-25. Status: not investigated yet; three hypotheses
ranked.

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

## Leading hypotheses, ranked

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

## Investigation plan

1. **Identify the asset.** Log the filename being loaded for the
   briefing-map texture. Find it on disk, open in an external
   viewer — confirm the raw file matches retail.
2. **Dump what reached the GPU.** Before the draw call, read back
   the texture surface (`glGetTexImage`) and save to PNG.
   - Black-patchy dump → **upload-side** bug (loader, format,
     palette). Instrument that path next.
   - Correct dump → **draw-side** bug (sampler, blend, alpha test).
     Try disabling alpha test and colorkey on that quad; if colors
     return, blend state is the culprit.
3. Bisection result determines the next direction.

## Cheap first check

Before full bisection: disable bilinear filtering on the map texture
only. If the blacks come back as correct colors, it's hypothesis 1.
Fix is either point-filter-only for keyed textures, or switching
colorkey to a real alpha channel in the asset.

(Same "alpha test with hard threshold discards soft edges" pattern
that broke the compass — see
`devlogs/closed/compass_investigation_2026-04-25.md` and
`memory/lesson_alpha_test_soft_alpha.md`. Possible the briefing map
is a variant of that class of bug.)

## Scope boundary

- Priority: higher than `mission_select_stray_lines_2026-04-25.md`
  (this one affects gameplay readability; that one is cosmetic).
- Don't conflate with the mission-select stray-lines bug. Different
  class entirely (texture/sampler vs geometry/coord).
- Don't bundle with any other branch.
