# Mission Selection — stray 1-px lines

**Started:** 2026-04-25. Status: not investigated yet; hypothesis from
handoff doc.

Promoted from `devlogs/followups/mission-select-stray-lines.md` (now
deleted — superseded by this devlog).

## Symptom

Stray thin primitives at wrong coordinates on the mission selection
screen:

- Yellow horizontal line on the far right edge, around mid-screen.
- Short pink/magenta line near the bottom, just left of the NEXT
  button.

Geometric, not texture corruption — real primitives submitted at wrong
coordinates.

## Leading hypothesis

Same class as the FMV-border `hiResOffsetX` double-application bug
fixed in commit `bc0d773`. The
`buttonlayout{1280,1600,1920}.fit` files pre-bake `HiresOffsets`
into `XLocation` values for some elements. If the engine then ADDS
the runtime offset on top of pre-baked coordinates, the element
lands double-offset — producing visible edges at the viewport
boundaries.

FMV fix pattern: snapshot raw `.fit` values into
`fitBakedHiResOffsetX/Y` and subtract them from the runtime offset
before use. If these mission-select primitives weren't touched by
the `controlgui.cpp` fix, same pattern likely applies.

## Investigation plan

1. Add a one-shot diagnostic in the 2D/UI vertex submission path
   (`tgl.cpp` or wherever FMV borders submit). For one frame on the
   mission-select screen, dump every quad whose width OR height is
   ≤ 2 pixels. Log coords, texture index, source if identifiable.
2. Cross-reference the dumped coords against
   `missionselectionscreen.cpp` and the active `buttonlayout.fit` at
   the test resolution (likely 1920) to identify which UI elements
   the stray primitives correspond to.
3. If they're `VideoStatic` or similar pre-baked-offset elements:
   apply the un-baking pattern from commit `bc0d773`.
4. If they're hardcoded dividers from C++: grep for the logged
   coordinates in source.

## Expected outcome

Fix pattern mirrors the FMV border work. Should be mechanical once
primitives are identified.

## Scope boundary

- Take after `briefing_map_black_textures_2026-04-25.md` since that
  bug is higher priority (briefing map is critical UI).
- Don't conflate this with the briefing-map bug — different root
  cause, different fix.
- Don't bundle with any other branch; this is a standalone fix.
