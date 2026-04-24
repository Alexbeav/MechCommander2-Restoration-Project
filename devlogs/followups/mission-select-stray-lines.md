# Mission Selection screen — stray 1-px lines

Priority: medium (cosmetic, not gameplay-blocking).

## Symptom

The mission selection screen shows stray thin primitives at the
wrong coordinates:

- A thin yellow horizontal line on the far right edge, around
  mid-screen.
- A short pink/magenta line near the bottom, just left of the NEXT
  button.

These are geometric, not texture corruption — actual primitives
submitted at wrong coordinates.

## Leading hypothesis

Same class as the FMV-border `hiResOffsetX` double-application bug
fixed in commit `bc0d773`. The `buttonlayout{1280,1600,1920}.fit`
files pre-bake `HiresOffsets` into `XLocation` values for some
elements. If the engine then ADDS the runtime offset on top of those
pre-baked coordinates, the element lands double-offset — which can
produce visible edges / stray lines at the viewport boundaries.

The FMV fix pattern was: snapshot raw `.fit` values into
`fitBakedHiResOffsetX/Y` and subtract them from the runtime offset
before use. If these mission-select primitives weren't touched by the
`controlgui.cpp` fix, same pattern applies.

## Investigation plan

1. Add a one-shot diagnostic log in the 2D/UI vertex submission path
   (likely `tgl.cpp` or wherever FMV borders submit). For one frame
   on the mission-select screen, dump every quad whose width OR
   height is ≤ 2 pixels. Log coords, texture index, source if
   identifiable.
2. Cross-reference the dumped coords against `missionselectionscreen.cpp`
   and the active `buttonlayout.fit` at the test resolution (likely
   1920 if running at 1920×1080) to identify which UI elements the
   stray primitives correspond to.
3. If they're `VideoStatic` or similar pre-baked-offset elements:
   apply the un-baking pattern from commit `bc0d773`.
4. If they're hardcoded dividers from C++: grep for the logged
   coordinates in source.

## Expected outcome

Fix pattern mirrors FMV border work. Should be mechanical once the
primitives are identified. Take after the briefing-map bug since
that's higher priority.

## Historical context

Shelved during the FMV work; originally captured in
`graphical_bugs_handoff.md` (since deleted, content migrated here).
