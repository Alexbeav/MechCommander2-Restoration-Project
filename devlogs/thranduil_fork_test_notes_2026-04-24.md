# ThranduilsRing fork test notes — 2026-04-24

Internal notes. If ThranduilsRing agrees to collaborate, this is the starting
backlog.

## Context

Tested `ThranduilsRing/mc2-opengl-remastered` v0.1 (release `v0.1-release`)
in parallel to our Restoration Project build, to evaluate collaboration scope.
Their fork modernizes the renderer (PBR splatting, tessellation, baked +
dynamic shadows, G-buffer, SSAO) and does not touch FMVs, the installer, or
most of our crash-fix territory.

## What their build gets right

- Terrain is noticeably crisper than vanilla.
- Load times significantly faster (likely a side effect of their renderer
  path being more efficient or lighter-weight than the original).
- Dynamic shadows on mechs — a real addition, even if too soft.
- Renderer modernization clearly functional — their README claims ~120 fps at
  4K on a current AMD card; consistent with what we observed.

## Gaps our fork already fills

If a merge happens, these are things our work brings to the combined project
that theirs currently lacks:

- **FMVs.** Their `code/mc2movie.cpp` is the unchanged 2001 Microsoft stub,
  and there are no Bink or FFmpeg references anywhere in their source tree.
  Intro, in-mission cinematics, mission briefings, pilot portraits, and
  credits do not play. Our FMV pipeline plugs this directly.
- **Mouse cursor.** Cursor behavior is broken in their build (specifics TBD
  after more time in-game). Our `bc0d773` hides the OS cursor while the
  game window is active.

## Regressions introduced by their fork

Things that work in both upstream `alariq/mc2` and our fork, but broke in
theirs — almost certainly caused by their renderer/audio rewrite:

- **Pilot "squawk" audio has an unpleasant distortion.** Mech pilot callouts
  have a new artifact that does not reproduce in alariq/master or our build.
  Probably a mixer-path change or sample-format regression from their audio
  rework. Worth diffing their audio changes against alariq to isolate the
  trigger.
- **Radar bandit icons missing.** The red minimap pings indicating enemy
  shape/size are gone. Significant tactical-UX loss. Works fine in upstream
  and in ours, so their renderer rewrite is dropping the draw calls —
  likely a minimap-layer or UI-pass regression.

## Shared roadmap — broken in both forks

Issues that predate both forks (inherited from upstream / the original
2001 code) and neither project has addressed yet. Already on our near-term
roadmap in this order:

- **Compass.** Non-functional in both forks. Next on our list after FMVs.
  Current diag probes live in `code/gamecam.*` and `mclib/*`.
- **Pre-mission map artifacts.** After compass.
- **UI scaling.** On our list after the pre-mission map. Our build's UI is
  already poorly scaled at higher resolutions; theirs is noticeably worse —
  in 4K the minimap is barely legible. Both need a resolution-aware scaling
  pass; PR #23 on `alariq/mc2` is our first stab at it.

## Their renderer work — concerns for a merged project

Not framed as "bugs in their fork" — framed as things a merged project would
need to weigh:

- **Grainy terrain.** Noise pattern similar to what you see *before* an AI
  denoiser completes. Probably their SSAO or another stochastic sampling pass
  lacks a smoothing/temporal-filter step. Worth investigating whether a TAA
  or blur pass would close the gap.
- **Fog-of-war boundary is unclear.** Hard to tell where visibility ends in
  their build. Their fog or visibility-mask pass has likely drifted from the
  original's sharp cutoff. May be a deliberate stylistic choice on their
  part — worth asking.
- **Dynamic shadow softness.** Their docs claim 16-sample Poisson PCF at
  4096×4096, but the result reads as mushy. Tighter kernel or higher shadow
  map resolution on key lights may be warranted. Don't need razor-sharp —
  just less diffuse than current.
- **Lighting changes original art direction.** The PBR pipeline shifts the
  mood of existing maps in ways that diverge from the original's painted
  palette. A merged project should consider an "original lighting" toggle,
  or at least tune per-mission to respect the original mission's intent.

## Not touched by either fork

- **Water shaders.** Still the 2001-era flat water. Their renderer work
  didn't address it, and ours had no reason to. Natural candidate for a
  merged project.

## Stretch: visual ambition items for a merged project

Things that would be great to land post-merge. Listed in roughly ascending
cost. Their renderer architecture makes all of these meaningfully cheaper
than they would be on vanilla.

- **Better smoke effects — tier 1: emitter additions.** Tie new particle
  emitters to SRM/LRM launch events + persistent battlefield timers.
  Doesn't require renderer changes; straightforward addition to the
  existing `mclib/gosfx/` particle system. Could be prototyped in our fork
  solo.
- **Better smoke effects — tier 2: soft-particle depth blending.** Use the
  G-buffer depth their pipeline already exposes so smoke doesn't clip
  flat against mechs — they push through it visibly. Small shader work on
  top of tier 1; requires their renderer.
- **Volumetric fog.** Screen-space ray-marched fog that samples shadow
  maps for god-ray / inscatter effect. Their existing "height-based fog"
  is the analytic precursor; volumetric is the next tier. G-buffer +
  shadow maps + post-process chain are all in place, so the lift is the
  shader itself.
- **Better smoke effects — tier 3: true volumetric.** 3D texture or SDF
  based volumetric smoke. Expensive but transformative for a mech game.
  Post-v1 ambition.

## Next step

Pending ThranduilsRing's response to the outreach issue. Two tracks:

**Our solo roadmap — continues regardless of collaboration:**

1. Compass (next after FMVs, already shipped).
2. Pre-mission map artifacts.
3. UI scaling (currently tracked on PR #23 upstream).
4. Anything else surfacing from community testing post-release.

**Additional items if collaboration happens** — their renderer regressions
and concerns a merged project would weigh:

- Fix their squawk-audio regression.
- Restore the radar bandit icons they dropped.
- Discuss terrain grain, fog-of-war clarity, shadow softness, and the
  lighting art-direction shift as collaborative design decisions.
- Stretch: water shader pass, since neither fork touched it.

If collaboration doesn't happen, we still benefit from their public work as
a reference for future renderer improvements, and the shared-roadmap items
stay on our plate either way.
