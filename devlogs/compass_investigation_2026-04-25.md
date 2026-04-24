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

Modify the existing `[COMPASS_DIAG]` probe at `tgl.cpp:2699` to log the
`theShape` identity (shape name, pointer, or parent-node name) alongside the
submission. Or split the counter into two: one for sky nodes, one for compass
nodes, by looking at `addFlags` plus a way to tell which `TG_Shape` we're in.

Cleanest approach: walk `msl.cpp:1686` where `listOfShapes[i].node->Render(...)`
is called with `isHudElement`, and tag the MultiShape with an ID/name the
probe can echo. The probe then reports per-MultiShape counts.

Expected output answers "is sky submitting 575 tris and compass submitting 2,
or is the compass contributing hundreds too?"

### Step 2 — pick a fix strategy based on Step 1's answer

**If the compass is the 2 triangles already drawn (sky dominates submissions):**
the compass pass is working; the visible bug is texture/state/coords inside
its 2 triangles. Investigate:
- `compass` appearance setup — what texture, what shape file
- GL state at :1585 (filter mode, alpha threshold)
- forceZ = 0.99999 → compass pass has `ZCompare=0`, should be fine — but
  verify no state leak from earlier passes.

**If the compass is contributing many triangles mis-routed into the solid
pass:** decouple the flags. Two options, roughly equal work:

- **Option A — split the flag.** Add `MC2_ISSKY` (next free bit: there's
  already `MC2_ISHUDLMNT=512` used for a different HUD path;
  `MC2_ALPHATEST=1024`, so 2048 is free). Have `theSky->setIsHudElement()`
  still apply the max-bright behavior but tag with `MC2_ISSKY` instead of
  `MC2_ISCOMPASS`. Sky keeps flowing through the solid/alpha passes (which
  currently work for it). Re-apply the `MC2_ISCOMPASS` exclusion in the
  solid pass — now safe because sky is not tagged.

- **Option B — decouple at the appearance level.** Add a boolean
  `isSky` parameter alongside `isHudElement` in the render call chain
  (`msl.cpp:1686`, `tgl.cpp:2527`). The max-bright code at `:1829` still
  fires for both; only the `MC2_ISCOMPASS` emission at `:2602` etc. is
  gated off for sky. Plumbing is deeper but no new flag bit needed.

Option A is less invasive — one flag bit, one call-site change
(`gamecam.cpp:634`), one exclusion re-add. Prefer it unless there's a
reason the TG_Shape path needs a parameter instead of a flag.

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
