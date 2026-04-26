# Night-mission rendering gaps

Status: open. Two visual-fidelity issues observed during pilot-video
runtime verification on a night mission, 2026-04-26. Pre-existing
behavior, not a regression from the FMV-review-fixes commit
(`27832e9`) or any recent work.

## Items

### Edge fog missing — black tiles at the screen edge

On at least one night mission, the screen edges show plain black
tiles instead of the soft fog that fills the edge band on
day missions (e.g. `m02 Reacquisition: Base Gemini`). Tiles "fill in"
visually only after the camera scrolls toward them, suggesting the
fog isn't being composited at all on this mission rather than just
being colored differently.

YouTube footage of original retail MC2 shows a "darkness" effect at
the edges on the same kind of night mission — a graceful fade-out,
not the day-mission fog but also not bare tiles. So we're missing
the night-specific edge treatment.

Likely areas:
- Terrain edge-of-world fog code (`terrain.cpp` / `terrobj.cpp`?)
- Time-of-day / lighting state — does the night flag get passed
  to the fog setup?
- A separate "edge darkness" pass that may have been gated out

Reproduction: load the night mission affected (need to identify
which one — was the next one after the user's last save when running
mc2.exe to verify the FMV review fix on 2026-04-26).

### Mech spotlights cast no ground illumination

On the same night mission, mechs visibly emit a spotlight from the
torso cone (the geometric cone is rendered) but the ground beneath
them stays dark — no light decal, no spotlight projection, no
shadowed cone. The spotlight should illuminate a circle/cone on the
terrain in front of the mech.

Retail behavior: ground lights are part of the night-mission
visual signature. The spotlight should land as a moving bright
patch as the mech walks.

Likely areas:
- Decal / projector system for spotlights
- Mech component that drives the spotlight (`mech.cpp` / `appear.cpp`?)
- A lighting pass / shader that may have been dropped during the
  GL port

Reproduction: night mission, watch any mech with the spotlight
visible. Compare to retail YouTube footage.

## Why one followup, not two

Both are night-mission lighting-pipeline gaps and likely share an
investigation surface (whichever code path renders the time-of-day
ambient/light setup). If the root causes turn out to be unrelated
they can split when promoted to active devlogs.

## Out of scope here

- Day-mission fog (which works correctly).
- General lighting overhaul / SDF / dynamic shadowing — separate
  much larger work if anyone wants to take it on.
- Any night mission that doesn't ship with these effects — start
  by confirming retail had them on the affected mission via
  reference footage.
