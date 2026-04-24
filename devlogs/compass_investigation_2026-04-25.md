# Compass investigation — 2026-04-25

Branch: `compass-fix`. Status: first fix attempt reverted; investigation open.

## Symptom

The in-game compass HUD is visually broken — either not rendering or rendering
with artifacts. `drawCompass` toggle (Ctrl+C) does fire through to the render
path; the compass object is allocated and `compass->render(-1)` is called each
frame. The problem is downstream — between triangle submission and screen.

## Initial hypothesis (external review, 2026-04-24)

Reviewer pointed at a three-way routing inconsistency in `mclib/txmmgr.cpp`:

- Solid pass at `:1049` matched `MC2_DRAWSOLID` without excluding `MC2_ISCOMPASS`.
- Alpha pass at `:1413` already excluded `MC2_ISCOMPASS` (plus `ISTERRAIN`,
  `ISSHADOWS`, `ISCRATERS`).
- Dedicated compass pass at `:1585` only saw what wasn't consumed earlier.

Fix applied in commit `e90749c`: mirror the alpha-pass pattern — exclude
`MC2_ISCOMPASS` from the solid pass so all compass geometry flows through the
dedicated pass.

## Why that fix was wrong (commit `b907ce4` revert)

In-game test immediately after the fix: white gaps appeared in the map around
mechs.

Root cause: **`MC2_ISCOMPASS` is misnamed.** It is the flag emitted for any
`TG_Shape::Render` call where `isHudElement == true` (tgl.cpp:2430, :2475,
:2604, :2722). Two places in the game call `setIsHudElement()`:

- `code/gamecam.cpp:619` → `compass->setIsHudElement()` (correct)
- `code/gamecam.cpp:634` → `theSky->setIsHudElement()` (also tagged!)

So `MC2_ISCOMPASS` really means "is HUD-class geometry rendered through the
TG_Shape path." Sky geometry picks up the flag too. Excluding `MC2_ISCOMPASS`
from the solid pass routed the sky's `DRAWSOLID` tris into the compass pass,
which uses HUD GL state (ZCompare=0, AlphaInvAlpha, Perspective=1). Sky polys
overdrew the world with no depth test → gaps around mechs.

The reviewer's pointer at a specific line was accurate; the diagnosis was
incomplete because the flag name lies about scope.

## What the probes proved

`[COMPASS_DIAG]` probes (commit `56d0411`, still live on this branch) logged
once per 60 frames. Pre-revert sample from `full_game/mc2_stdout.log`:

```
[COMPASS_DIAG] TG_Shape::Render submit: turn=60 tris_added=577 flag=DRAWSOLID|ISCOMPASS texIdx=1201 z=0.99999
[COMPASS_DIAG] render(new): turn=60 compass=alloc drawCompass=1 gate=PASS
[COMPASS_DIAG] renderLists compass pass: turn=60 nodes=2 verts_seen=6 verts_drawn=6 nextAvailableVertexNode=46
```

- ~577 triangles tagged `DRAWSOLID|ISCOMPASS` are submitted per 60 frames.
- The compass pass sees only 2 nodes / 6 vertices (= 2 triangles).
- The other ~575 triangles are consumed by the solid pass at `:1049`, which
  then resets `currentVertex`, leaving the compass pass with empty lists for
  those nodes.
- `z=0.99999` (forceZ value) — everything is being pushed to the far clip
  plane. With solid-pass `ZCompare=1`, compass tris at z ≈ 1.0 fail the
  depth test against anything already written with a smaller z.

## What we still don't know

The probe aggregates the full submission count per tick — **compass vs sky
share the counter.** From the numbers alone we can't say whether:

- (a) the compass contributes most of the 577 tris and is mostly being
  consumed wrongly by the solid pass, or
- (b) the sky contributes ~575 tris (large dome) and the compass is the 2
  triangles the compass pass already sees correctly — meaning the actual
  compass bug is elsewhere (alpha state, texture, coordinates).

Either is consistent with the current data. The next probe has to break out
the submissions per shape (compass vs sky) to pick.

### Update 2026-04-25 late: `z=0.99999` in the probe is sky, not compass

The probe's last-submission-wins design picks whichever shape submitted
last within a 60-tick window. Tracing the render calls:

- `gamecam.cpp:150` — `theSky->render(1)` → `GenericAppearance::render`
  `depthFixup > 0` → `bldgShape->Render(false, 0.9999999f)` → **sky verts
  get forceZ = 0.9999999** (far clip).
- `gamecam.cpp:180` — `compass->render(-1)` → `BldgAppearance::render`
  `depthFixup < 0` → `bldgShape->Render(false, 0.00001f)` → **compass
  verts get forceZ = 0.00001** (near clip).

So the `z=0.99999` in the logged sample is a sky submission, not a compass
one. Compass submissions are happening; the probe just never captured one
because sky almost always submits more triangles in a 60-frame window and
wins the "last submit" race.

This reinforces that scenario (b) is plausible — compass might only
contribute a handful of tris, already visible in the `nodes=2, verts=6`
the compass pass sees. The bug may be inside that pass (state, texture,
coords) rather than at the routing level.

## MS original vs our fork (compared 2026-04-25)

The original 2001 Microsoft source (`MC2_Source_Code/Source/MCLib/txmmgr.cpp`
in the reference checkout) has the **same routing structure** as our fork:

- MS solid pass at `:518` — matches `DRAWSOLID` with no `ISCOMPASS`
  exclusion. Same as our fork's `:1047`.
- MS alpha terrain pass at `:575` — requires `ISTERRAIN`, implicitly
  excludes ISCOMPASS.
- MS general alpha pass at `:803` — excludes `ISTERRAIN`, `ISSHADOWS`,
  **`ISCOMPASS`**, `ISCRATERS`. Same as ours.
- MS dedicated compass pass at `:964` — matches `ISCOMPASS`. Same as ours.

And MS `tgl.cpp` emits the same `MC2_DRAWSOLID | MC2_ISCOMPASS` flag
combination for HUD-element solid geometry (MS:2367, ours:2688). So the
"compass geometry arrives in the solid pass and gets drained before the
compass pass sees it" pattern existed in MS too. It worked there.

**The bug is therefore NOT the three-way routing inconsistency the
reviewer pointed at.** It is something that changed between MS's pipeline
and ours that breaks the visible compass without breaking the sky.

The shader-based hardware path added in alariq commit `71b9bdd` (May 2018)
was the first regression suspect — it adds a new `masterHardwareVertexNodes`
loop at `txmmgr.cpp:982` that also drains `DRAWSOLID` without ISCOMPASS
exclusion. **But ruled out for this bug:** `bShadersDrawPathEnabled` is
initialized `false` at `tgl.cpp:98` and never flipped, so the hardware
path is currently a no-op.

Remaining regression candidates (none yet tested):

- **OpenGL clip-space Z semantics.** MS used DirectX 8 (Z 0..1); the
  port uses OpenGL (default Z -1..1). If `forceZ = 0.00001` for the
  compass doesn't land where expected post-projection, the compass
  could be clipped by the near plane instead of landing in front.
- **`theClipper->StartDraw` / clip-plane changes.** Terrain render
  runs between sky and compass (`gamecam.cpp:152`) and could leave
  clip-plane state that affects near-plane compass tris.
- **Texture state leak into compass pass.** The solid pass sets
  `TextureWrap` / `TextureClamp` but not alpha mode. If a prior pass
  leaves alpha state incompatible with a translucent compass texture,
  the compass could render as an opaque bounding quad (black square
  around the compass area) — which could read as "broken" depending
  on how it looks in-game.
- **`BldgAppearance` setup for the compass.** `gamecam.cpp:584`
  creates it as `new BldgAppearance` and drives it through the
  building render pipeline. If any building-specific code path
  changed (e.g. shadow/occlusion logic, highlight color blending,
  `setObjStatus(OBJECT_STATUS_DESTROYED)` interaction) the compass
  could be masked or invisible without the flag routing being
  implicated.

## Why flag-level exclusion can't fix this cleanly

`isHudElement` controls two behaviors in `TG_Shape::Render`:

1. `tgl.cpp:1829` — clamps vertex color to max-bright (unlit HUD look).
2. `tgl.cpp:2428/2473/2602/2720` — emits `MC2_ISCOMPASS` in the flag.

Both apply to compass and sky alike. Sky needs the max-bright behavior (it's
unlit) but should *not* be routed through the HUD-state compass pass. The two
concerns are tangled under one flag.

Any fix that touches the pass routing has to first decouple compass from sky.

## Next investigation steps

### Step 1 — sharper probe (non-invasive)

Modify the existing `[COMPASS_DIAG]` probe at `tgl.cpp:2699` so it splits
per-z-range: submissions with `z < 0.5` go to a "compass-likely" counter,
submissions with `z > 0.5` go to a "sky-likely" counter. Log both every
60 frames without resetting inner counters.

Also add a probe INSIDE the solid pass at `txmmgr.cpp:1049` that logs,
per frame, how many vertices with `MC2_ISCOMPASS` are consumed by that
pass — that tells us directly whether compass verts are being drained
there vs. reaching the compass pass.

Expected outputs answer two things at once:
- How many tris is the compass actually submitting?
- Is compass geometry being drained by the solid pass (flag-routing
  regression) or reaching the compass pass intact (regression
  elsewhere — Z clip, alpha state, texture, BldgAppearance setup)?

### Step 2 — pick a fix strategy based on Step 1's answer

**If the compass reaches the compass pass correctly (nodes/verts count
matches submission count):** the compass pass is working as designed but
the output isn't visible. The bug is inside the compass pass itself.
Investigate in this order:
- Does the OpenGL Z-clip differ from DirectX? forceZ=0.00001 might be
  clipped by the near plane in OpenGL default `glDepthRange(0,1)` if
  projection remaps to -1..1 clip space. Try forceZ=0.01 or 0.1.
- GL state at the compass pass (`txmmgr.cpp:1580-1640`) — filter mode,
  alpha threshold, ZCompare/ZWrite. Especially state leaked from the
  preceding effects/spotlight passes.
- `BldgAppearance` compass setup — what texture, what shape does the
  compass appearance point at? Check
  `appearanceTypeList->getAppearance(BLDG_TYPE << 24, "compass")`.

**If the compass is being drained by the solid pass:** decouple compass
from sky, then re-apply the exclusion. Two options:

- **Option B (cheapest, most informative) — don't call `setIsHudElement`
  on sky at all.** Remove the call at `gamecam.cpp:634`. Test whether
  sky still renders correctly without the max-bright vertex-color clamp.
  If yes, this is the smallest possible change to decouple the two —
  we can then safely re-apply the solid-pass `ISCOMPASS` exclusion.

- **Option A (fallback if sky loses max-bright) — split the flag.** Add
  `MC2_ISSKY` (next free bit: `2048`). Have `theSky->setIsHudElement()`
  still apply the max-bright behavior but tag with `MC2_ISSKY` instead
  of `MC2_ISCOMPASS`. Sky keeps flowing through the solid/alpha passes;
  compass flows through its dedicated pass.

Start with Option B — it's one line of code. If sky still looks right,
re-attempt the `MC2_ISCOMPASS` exclusion in the solid pass.

### Step 3 — verify the actual compass fix works

After Step 2: rebuild, test in a mission, check that compass renders cleanly
AND sky still has no gaps. Then strip the `[COMPASS_DIAG]` probes before
merging to master (same pattern we used for the pilot-video probes — see
commits `575afc6` / `787cbb0`).

## Code pointers

- Flag definitions: `mclib/txmmgr.h:52-62`
- HUD-element flag set: `mclib/msl.h:522`
  (`SetIsHudElement` on `MultiShape`)
- Appearance-level wrappers: `mclib/bdactor.h:356`, `mclib/genactor.h:241`
  (both call `bldgShape/genShape->SetIsHudElement()`)
- Call sites that flip it on: `code/gamecam.cpp:619` (compass),
  `code/gamecam.cpp:634` (sky)
- Max-bright behavior: `mclib/tgl.cpp:1829`
- Flag emission in submission path: `mclib/tgl.cpp:2428`, `:2473`, `:2602`,
  `:2720` (all gated on `isHudElement`)
- Solid pass (drains `DRAWSOLID`): `mclib/txmmgr.cpp:1047-1101`
- Alpha pass (excludes `ISCOMPASS`): `mclib/txmmgr.cpp:1420-1465`
- Dedicated compass pass: `mclib/txmmgr.cpp:1585-1636`
- Dedicated HUD-element pass (different flag, `MC2_ISHUDLMNT`, for bitmap
  overlays like building selection boxes + mech hold-fire icon):
  `mclib/txmmgr.cpp:1642-1682`
- Probe code (kept for next iteration):
  - `code/gamecam.cpp:170` — render gate probe
  - `code/gamecam.h:130` — toggle probe
  - `mclib/tgl.cpp:2699` — submission probe (needs per-shape split)
  - `mclib/txmmgr.cpp:1638` — compass-pass probe

## Lesson for memory

`MC2_ISCOMPASS` is a flag-name trap — it's set for any isHudElement-
flagged TG_Shape, which includes the sky. Flag-level exclusions around it
cannot distinguish compass from sky. Anyone else tempted to "fix" the
routing by excluding `MC2_ISCOMPASS` from a pass needs to decouple sky
first.
