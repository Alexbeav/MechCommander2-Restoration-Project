# Mission briefing map — mostly black

Priority: high (briefing map is critical UI).

## Symptom

The mission briefing map renders mostly black. Water and buildings are
pure black; only some darker terrain patches (green/brown) are
visible. Retail reference shows water (blue), buildings (light grey),
and terrain in natural colors — all on the same map.

The red selection marker and its "1" label still render correctly on
top, so the overlay sprite/text path is healthy. The problem is
localized to the map texture itself.

## Leading hypotheses, ranked

1. **Color-key leak with bilinear filtering.** MC2 uses a colorkey for
   transparency (commonly magenta `0xFF00FF`; some assets use others).
   If the map asset's key is close to the blue water or light-grey
   building colors, and bilinear filtering samples *before* the
   colorkey comparison, edge pixels sampling between water and a
   key-adjacent color will all match the key and vanish. Classic
   bilinear + colorkey footgun.
2. **Palette / indexed texture.** If the map is 8-bit palettized and
   its palette chunk isn't loaded (wrong offset, missing chunk-ID
   handling), most indices land on black and only a few map to
   visible colors.
3. **Channel / format mismatch.** BGR vs RGB swap, or DXT1 (1-bit
   alpha, zero tolerance) upload path when the loader expected
   uncompressed.

## Investigation plan

1. **Identify the asset.** Log the filename being loaded for the map
   texture in the briefing / mission-selection render path. Find it
   on disk, open in an external viewer — confirm the raw file matches
   the retail reference.
2. **Dump what reached the GPU.** Right before the draw, read back
   the texture surface (`glGetTexImage` or equivalent) and save to
   PNG.
   - Dumped PNG is black-patchy → upload-side bug (loader, format
     conversion, palette). Instrument that path next.
   - Dumped PNG is correct → draw-side bug (sampler, blend, alpha
     test). Try disabling alpha test and colorkey on that quad; if
     colors return, colorkey/blend state is the culprit.
3. Bisection result determines the next direction.

## Cheap first check

Before the full bisection, try disabling bilinear filtering on the
map texture specifically. If the blacks come back as correct colors,
it's hypothesis 1. Fix is either point-filter-only for keyed
textures, or switch colorkey to a real alpha channel in the asset.

(The same "alpha test with hard threshold discards soft edges" pattern
burned us on the compass — see `devlogs/closed/compass_investigation_2026-04-25.md`
and `memory/lesson_alpha_test_soft_alpha.md`. Possible the briefing
map is a variant of the same class of bug.)

## Historical context

Shelved during the FMV work; was originally captured in
`graphical_bugs_handoff.md` (since deleted, content migrated here).
