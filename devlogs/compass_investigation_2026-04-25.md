# Compass investigation — 2026-04-25

Branch: `compass-fix`. Status: flag-level fix reverted; root cause narrowed to
a D3D8→OpenGL XYZRHW translation bug in `shaders/gos_tex_vertex.vert:17`.
Not yet patched — want an external review of the proposed fix before trying
it (previous "fix" broke sky).

## TL;DR (2026-04-25 end-of-day)

`MC2_ISCOMPASS` is a HUD-element flag that tags BOTH the compass and the
sky. The reviewer's pass-routing fix reverted because it broke sky.

Probes then proved the compass geometry reaches the dedicated compass pass
intact (6 verts / 2 tris per frame, correct screen coords (459-1371, 311-740)
within viewport 1707×960, valid texture handle 0xf4, valid UVs, full alpha).
`gos_RenderIndexedArray` is called on those verts. Nothing appears.

The rhw probe showed rhw ≈ 0.000924 on the compass verts (= 1/w_clip with
w_clip ≈ 1082). In `shaders/gos_tex_vertex.vert:17`, the GL vertex shader
does:

```glsl
gl_Position = mvp * vec4(pos.xyz, 1) / pos.w;
```

Where `pos.w` is the vertex attribute's w = `gos_VERTEX::rhw`. For
pre-transformed (D3D8 XYZRHW-style) compass verts, that divide scales the
NDC by 1082 and the primitive gets clipped off-screen.

Sky also uses the same path but its `rhw` is near 1.0 (coincidentally — sky
dome geometry is designed such that post-projection w ≈ 1), so the divide
doesn't break it.

**Proposed fix candidates (none applied yet, want review):**

1. Remove the `/ pos.w` divide in `gos_tex_vertex.vert` (and its `_lighted`
   sibling if similar). Risks regressing whatever currently relies on the
   divide.
2. At the forceZ override site (`mclib/tgl.cpp:2660`), also set
   `gVertex[i].rhw = 1.0f` when forceZ is near plane (compass case). Risks
   breaking perspective-correct texture sampling on the compass, though
   for a flat near-plane quad this is arguably fine.
3. Write rhw = 1.0 at the compass-specific submission site, not at the
   shared forceZ path. Least risk; slightly ugly.

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

## Update — what Step 1 probes actually told us (2026-04-25 evening)

Three additional probes were added on the `compass-fix` branch (commits
`4842291`, `43e92e7`, `4d04b9b`). Data from the log:

**Submission split probe** (near-plane vs far-plane, at tgl.cpp:2692):

```
turn=60..900  near_tris=120 (≈ 2 tris/frame, texture 1203, z=1e-05)
              far_tris=480  (≈ 8 tris/frame, texture 1201, z=0.99999)
```

- `near_tris=120/60f = 2 tris/frame` → compass (forceZ=1e-05 at near plane)
- `far_tris=480/60f = 8 tris/frame` → sky (forceZ=0.9999999 at far plane)
- After compass toggle-off: `near_tris=0`. Confirms the 2 tris/frame is
  the compass alone, not interference from other HUD elements.

**Solid pass drain probe** (at txmmgr.cpp:1047):

```
turn=60   solid pass drained ISCOMPASS: nodes=1 verts=24
turn=240  solid pass drained ISCOMPASS: nodes=2 verts=24
```

- 24 verts drained per frame = 8 tris = sky submissions.
- Compass submissions (6 verts, 2 tris) are NOT drained here.
- Explanation: the compass texture at node 1203 has alpha, so compass tris
  are tagged `DRAWALPHA|ISCOMPASS` instead of `DRAWSOLID|ISCOMPASS`. The
  solid pass only matches `DRAWSOLID`, so compass falls through.
- Good: compass gets to the dedicated compass pass with its 6 verts intact.

**Compass pass probe**:

```
turn=60..  nodes=2-3 verts_seen=6 verts_drawn=6
```

- Compass pass sees the compass verts (6) and calls gos_RenderIndexedArray
  on them. Still nothing visible.

**Vertex sample probe** (at the compass pass, with rhw):

```
turn=60 texNodeIdx=1203 texHandle=0xf4 n=6
        v0=(459.256, 739.378, 1e-05 rhw=0.000924461 argb=0xffffffff uv=0.0005,0.9995)
        v1=(1371.19, 373.434, 1e-05 rhw=0.000759132)
        v2=(682.697, 311.477, 1e-05 rhw=0.000731141)
        viewport=1707x960
```

- XY coords span (459-1371, 311-740) — well within 1707×960 viewport.
- Z = 1e-05 (forceZ, near plane).
- argb = 0xffffffff (opaque white).
- UV = (0.0005, 0.9995) — sampling the full texture.
- texHandle = 0xf4 — valid GL texture.
- **rhw ≈ 0.000924** — non-zero, so not a divide-by-zero NaN.

All the post-projection data looks correct, yet the vertex disappears.

## Root cause — OpenGL shader's XYZRHW mistranslation

At `shaders/gos_tex_vertex.vert:17`:

```glsl
layout(location = 0) in vec4 pos;    // bound to gos_VERTEX::{x,y,z,rhw}
uniform mat4 mvp;

void main(void) {
    vec4 p = mvp * vec4(pos.xyz, 1);
    gl_Position = p / pos.w;          // pos.w == rhw here
    ...
}
```

The `mvp` uniform is set to `projection_` from
`gameos_graphics.cpp:1291`, which is a screen-space → NDC ortho:

```
x' = 2x/W - 1
y' = 1 - 2y/H
z' = z
w' = 1
```

That correctly maps screen coords (459, 739) into NDC (~-0.462, -0.54).
Then the shader divides by `pos.w = rhw`.

For the compass: rhw ≈ 0.000924. NDC / 0.000924 = NDC × 1082. The vertex
ends up at (~-500, ~-585, ...) in NDC space — way outside [-1, 1] — and
the whole primitive is clipped.

D3D8 XYZRHW semantics expect the rhw field to be used for
perspective-correct texture sampling, NOT to re-divide the already
pre-transformed position. The `/ pos.w` in the OpenGL port looks like a
port-era translation error: someone assumed `w` is a normal clip-space w
that needs the perspective divide, missing that these verts are already
post-divide.

### Why sky isn't broken by the same shader

The sky verts go through the same code path (isHudElement → ISCOMPASS
flag → solid pass at txmmgr.cpp:1047 → `drawIndexedTris` →
`selectBasicRenderMaterial` → `gos_tex_vertex.vert`). The rhw field on
sky verts is not logged by our probes, but we know sky renders correctly
post-revert. The most likely explanation:

- Sky dome geometry is designed such that its post-projection clip-space
  `w ≈ 1`. This gives `rhw ≈ 1`, and `NDC / 1 = NDC` → renders fine.
- Compass is a `BldgAppearance` whose shape geometry spans ~1000 world
  units. Post-projection clip-space `w ≈ 1082`, `rhw ≈ 0.000924`, divide
  pushes NDC off-screen.

Haven't confirmed sky's rhw with a probe. Could add one as verification
step — but the arithmetic makes this explanation straightforward.

### Why the same source compiled and worked on MS D3D8

D3D8's `D3DFVF_XYZRHW` flag told the fixed-function pipeline: "these
vertices are already in screen space; use rhw only for texture perspective
correction, and treat the position as-is." The OpenGL port rewrote that
as a vertex shader but incorrectly included the rhw divide on position.
MS's compass worked because D3D8 didn't do the divide; ours doesn't
because OpenGL does.

## Proposed fix directions (not yet applied)

None have been tried. Given the previous "fix broke sky" outcome, the
bar is higher: we want a second pair of eyes before any patch.

**Option 1 — Fix the shader.**

Change `shaders/gos_tex_vertex.vert:17` from:
```glsl
gl_Position = p / pos.w;
```
to:
```glsl
gl_Position = p;
```

Also check `shaders/gos_tex_vertex_lighted.vert` and any other shaders
selected by `selectBasicRenderMaterial` / `selectLightedRenderMaterial`
for the same pattern.

Risk: anything that currently renders CORRECTLY relying on the divide
(sky, maybe some world geometry) would break. This is the real concern —
sky clearly works today with `rhw ≠ 1`, so either the "rhw≈1 coincidence"
theory above is wrong, or our model is incomplete. Before this fix we
should log sky's rhw too, to confirm.

**Option 2 — Fix rhw at the forceZ site.**

At `mclib/tgl.cpp:2660` where forceZ is written to z:
```cpp
if ((forceZ >= 0.0f) && (forceZ < 1.0f))
{
    gVertex[0].z = forceZ;
    gVertex[1].z = forceZ;
    gVertex[2].z = forceZ;
    // Also reset rhw to 1.0 so the shader's divide is a no-op. These
    // vertices are pre-projected HUD overlays; perspective-correct
    // texture sampling doesn't apply.
    gVertex[0].rhw = 1.0f;
    gVertex[1].rhw = 1.0f;
    gVertex[2].rhw = 1.0f;
}
```

Risk: sky ALSO uses forceZ (=0.9999999). If sky's current `rhw ≠ 1`
is actually correct (e.g. used for perspective-correct texture mapping
of the sky dome), overriding it to 1.0 would distort sky textures.
Mitigation: gate by `isHudElement` explicitly but only for near-plane
overrides (forceZ < some threshold), or pass a flag down.

**Option 3 — Fix rhw only when the source is unambiguously compass.**

At `gamecam.cpp:180` / `BldgAppearance::render` for the compass, set a
flag on the shape (like `renderAsScreenSpaceHud`) that propagates to the
submission and forces rhw=1.0. Narrowest fix, most plumbing.

## Questions for the evaluator

Things we'd especially like a second pair of eyes on:

1. Is the shader's `gl_Position = p / pos.w` definitely a bug? Or is
   there a compensating transform elsewhere that we've missed? Places
   we've checked:
   - `mclib/tgl.cpp:1696-1719` (XYZRHW vertex setup for all TG_Shape
     submissions — sets `rhw = 1.0 / xformCoords.w`).
   - `mclib/tgl.cpp:2660` (forceZ override — touches z only).
   - `GameOS/gameos/gameos_graphics.cpp:1291` (`projection_` ortho
     matrix for screen→NDC).
   - `gameos_graphics.cpp:1824` (`mat->setTransform(projection_)`
     before the drawIndexedTris path).

2. Why is sky not broken by the same divide? Our "rhw coincidentally ≈ 1"
   hypothesis is unverified. Would probing sky's rhw settle it, or is
   there a sky-specific code path we've overlooked?

3. Of the three fix options, which is lowest risk given alariq's intent
   in `71b9bdd` (initial shader-based drawing commit, May 2018)? We've
   looked at that commit; it doesn't show rationale for the `/ pos.w`.

4. Are there other rendering artifacts we'd expect from the same bug
   but haven't noticed (e.g. skewed FX sprites, misplaced HUD bitmaps)?
   If so, the narrow rhw-only-for-compass fix might be hiding a
   broader issue.

## Next action

Pause here pending evaluator review. If we proceed without review:
first add a sky-rhw probe to confirm/disprove the "rhw ≈ 1" hypothesis,
then pick Option 2 gated by an additional condition (compass-specific,
not all forceZ uses).

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
