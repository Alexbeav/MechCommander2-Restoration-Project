# Project state

Living snapshot of current branches, external collaboration, and open
work. Refreshed when something material changes — not on a calendar.

Last update: 2026-04-27 (alariq closed his fork as an upstream
channel — stated 0% LLM-code policy and considers his project
"finished"; all six of our open PRs there closed; positioning
revised to "stay independent, cherry-pick/share opportunistically").

## Positioning — "MC2 vanilla plus, cross-OS"

The project's shape is **MechCommander 2 vanilla plus** with **cross-
OS reach (Windows, Linux, Mac)** — the definitive modern way to play
the 2001 retail campaign on whatever OS you're on, with quality-of-
life fixes and compatibility work on top. Not a total conversion, not
a reimagining, not a remaster (that's ThranduilsRing's direction —
visual fidelity, Windows-first, ships upscaled data).

**Our `master` is the project's authoritative branch.** We are not
downstream of any other fork. Bug fixes flow opportunistically with
peer forks (ThranduilsRing actively, Methuselas pending), but no
tree-convergence relationship and no outbound PR queue.

Focus is own-fork work on the vanilla experience:

- Font rendering quality (D3F loader + visual-bounds API landed
  2026-04-26; menu fonts ✓; tracked residuals at issue #1 +
  `devlogs/followups/font-tweaks-residual.md`)
- UI resizing / repositioning for modern resolutions
- Pre-mission briefing map bug fixes
- Mission editor revival so the community can author custom missions
  (deferred — adopting Methuselas's work when he publishes it)
- Crash / stability fixes (FMV pipeline done; more as they surface)
- Installer that patches over a legal retail copy (no asset
  redistribution; license anchor via `Mc2Rel.exe`)
- Cross-OS compat work as it surfaces (Linux already works via
  alariq's port; Mac is future via MoltenVK / RHI)

## Active branches

- `master` — shipping baseline. As of 2026-04-26 includes everything
  through the prior baseline plus: D3F font loader + ASCII-restricted
  calibration + `.glyph` sidecar bridge (`0a1ed6b`), visual-bounds
  centering API + 3 widget rewires (`3a0bb12`), `+1` bias drop
  (`a9e7b46`), devlog closure + followup spawn (`4202448`), PR-review
  fixes for the upstream PR (`2acd680`: ink-array leak, GL_UNPACK
  alignment save/restore, gos_GetTextureGLId handle validation,
  visual-bounds leading-newline bug, legacy fallback off-by-one,
  gos_load_glyphs EOF guard), FMV/restoration review fixes
  (`27832e9`: Linux portability, pilot-video defensive cleanup,
  instrumentation cleanup, header guard rename), and night-mission
  rendering followup spawn (`8528ce2`).
- `input-fixes` — 4 commits ahead of master. `gos_SetMousePosition`
  coord fix, SDL `SDL_SetWindowMouseGrab` at window creation, grab re-
  assertion on focus events + proper `SDL_WINDOWEVENT` dispatch, and
  Win32 `ClipCursor` reinforcement. Tested. **Known minor residual:**
  on one 3-monitor setup the cursor can escape by ~1 column of pixels
  to the adjacent monitor without losing focus; likely environmental
  (another process calling `ClipCursor`, DPI-seam quirk, or monitor-
  alignment utility) rather than a code-side miss.
- `editor-tier0` — empty branch off master, ready for when editor work
  starts. Step 0 of that work is unignore + commit `MC2_Source_Code/`
  (currently in `.gitignore`). Plan details in
  `devlogs/followups/mission-editor-tier0.md`.
Retired (ff-merged into master):

- `fonts` — 2026-04-26. All commits now on master via fast-forward.
- `fmv-review-fixes` — 2026-04-26. All commits now on master.
- `fonts-upstream` — 2026-04-27. Existed only to host upstream PR
  alariq/mc2#32; PR closed, branch can be deleted.

Retired (squash-merged into master):

- `compass-fix` — 2026-04-25 as `379c8d5`. Root cause was an alpha-test
  fragment discard in `shaders/gos_tex_vertex.frag` killing soft-alpha
  compass pixels. Investigation trail at
  `devlogs/closed/compass_investigation_2026-04-25.md`.
- `fmv-mp4-support` — 2026-04-24.
- `fmv-mp4-support-backup` — user's personal safety branch, untouched.

## External collaboration

**alariq** — channel **closed 2026-04-27**. He stated a 0% LLM-code
policy ("AIs ... are not welcome here") and considers his fork
"finished for now ... all functionality (except networking) is
implemented." All six of our open PRs (#23, #24, #26, #28, #29, #32)
were closed by us as a result; the work continues on our master.
We can still cherry-pick **from** his master if he commits anything
useful — relationship is now opportunistic-inbound only, no comms.
Durable note: `memory/project_alariq_upstream_closed.md`.

**Methuselas** — collaboration pending. Per email 2026-04-27 he has:
working save/load with round-trip integrity, water-bug fix, fog fix,
OG-map load/save, Mission/Team Settings, Campaign Editor — i.e. Tier 0
mission-editor work plus extras already done on his fork. He's offered
to merge our work (FMV, fonts, input-fixes, compass, briefing-map, the
queue alariq just closed) into his fork. Blocker on his side: he has
no optical drive on his dev box and can't extract MC2 retail data.
Awaiting him to push his branch publicly before we evaluate
integration cost. Open question on our side: how to handle the data-
sharing ask (MS's 2006 release covers code, not data).

**ThranduilsRing** (maintainer of
[`mc2-opengl-remastered`](https://github.com/ThranduilsRing/mc2-opengl-remastered))
— positioning locked publicly in discussion #2: our two projects are
genuinely orthogonal (vanilla+ with cross-OS reach vs. Windows-first
remaster) and coexist as different surfaces for different users. Bug
fixes flow both ways without either fork adopting the other's scope.
Discussion thread:
<https://github.com/ThranduilsRing/mc2-opengl-remastered/discussions/2>.

**Current state (2026-04-25 eve):**

- His **v0.2 release** shipped 2026-04-25 with cinematics (independent
  FMV impl converged on the same engineering choices as ours —
  audio-master A/V clock with wall-clock fallback, gos-owned texture,
  letterboxed screen-space quad — but with a cleaner `mc2video` opaque
  session factoring), an ASan harness, smoke-test runner with
  tier1/tier2 gates, and three base-game crash fixes from ASan finds.
- **Three of his base-game fixes cherry-picked to our master with `-x`
  attribution and pushed to origin:** `eb7afb2` (Turret teamId guard),
  `bcdbb4b` (ABL code-segment sentinel), `4c5de32` (GlobalMap
  pathExistsTable zero-init). All single-file, surgical, real latent
  bugs in alariq's base.
- **His repo cloned at `F:/games source code/games/mc2-opengl-remastered/`**
  and tracked as `thranduil` remote in our mc2 repo.
- **Six MC2-foundational lessons** pulled from his sanitized
  [mc2-claude-memories](https://github.com/ThranduilsRing/mc2-claude-memories)
  into our memory dir (texture-handle-is-live, ARGB-is-BGRA, init-order,
  tgl-pool, 8-bit-WAV-unsigned, clip.w-sign-trap).
- **Discussion #2 reply posted** with positioning lock and offers
  cherry-pickable patches (`sws_scale` tail-guard, `bMovie` UAF, three
  smaller FMV fixes from `9e03335`); awaiting his read.

**Plan now (no tree convergence — coexist + cross-pollinate):**

1. Cherry-pick his fixes selectively when they're orthogonal to scope;
   send him our crash fixes as cherry-pickable patches. No rebases.
2. Diff `9751e86` (his HUD-inverse mouse transform fix) against our
   `input-fixes` branch — overlaps with our cursor work; needs review
   before applying.
3. Verify whether our `gameos_sound.cpp:297` has the `AUDIO_U8` fix
   from his memory (action item flagged in MEMORY.md).
4. Adopt his ASan harness as a process upgrade when we have a quiet
   slot.

**Installer decision (resolved):** keep as the canonical vanilla+
distribution surface. Patches over a legal retail copy via `Mc2Rel.exe`
license anchor; no asset redistribution. Distinct from his zip-bundle
all-in-one v0.2 surface. Different users, no conflict.

**Cross-OS / Mac pathway (not urgent):** OpenGL → RHI → Vulkan with
MoltenVK on Mac. RHI as intermediate layer first, decouples renderer
from backend. Could be an inbound contribution to his remaster too if
he wants Mac reach.

## Upstream PRs (closed)

All six PRs we had open on `alariq/mc2` were closed 2026-04-27 in
response to alariq's stated AI policy. No outbound PR queue is being
maintained; if a peer fork opens its own contribution channel we'll
evaluate it then.

Closed PRs (work landed on our master):

- #23 UI scaling
- #24 Windows build improvements
- #26 FMV pipeline (FFmpeg/MP4)
- #28 Campaign-dialog duplicate fix
- #29 Compass alpha-test fix
- #32 D3F font loader + visual-bounds API

`devlogs/followups/alariq-pr-waits.md` is now obsolete; it can be
moved to `devlogs/closed/` as a historical artifact next time we
touch followups.

## External reviewer findings (2026-04-24)

- **High — `setMousePos` coord bug** at `mclib/userinput.h:419` →
  `GameOS/gameos/gameos_input.cpp:78`. **Fixed** on `input-fixes`.
- **Medium — compass multi-pass routing** in `mclib/txmmgr.cpp`.
  **Resolved 2026-04-25 but not via that routing.** Routing was correct;
  real bug was alpha-test fragment discard. Landed as `379c8d5`.
- **Medium — editor source gitignored** at `.gitignore:97` while
  `MC2_Source_Code/Source/Editor/EditorMFC.vcproj:4` exists. **Open** —
  step 0 of editor-tier0.
- **Low — `.bik` strip via `strstr`** in `code/logistics.cpp`. **Fixed**
  on `master`.

## Next concrete moves

1. **Pick up one of the active devlogs in `devlogs/` root** — both
   pre-existing investigations parked when fonts took priority:
   - `briefing_map_black_textures_2026-04-25.md`
   - `mission_select_stray_lines_2026-04-25.md`
2. **Font residuals** (issue #1). Top-aligned screen headers
   (`MECH BAY`, `MISSION SELECTION`, etc.) sit a few px too high —
   different render path than the centering rewires we just landed.
   Smaller-scope continuation while font context is fresh.
3. **Cherry-pick `9751e86` (Thranduil's HUD-inverse mouse transform
   fix) onto `input-fixes`** — overlaps with our cursor work; needs a
   real diff before applying.
4. **Verify `AUDIO_U8` fix in `GameOS/gameos/gameos_sound.cpp:297`** —
   action item from imported memory; if missing, port the one-liner.
5. **Merge `input-fixes` to master** when validated (residual
   1-pixel-column on 3-monitor likely environmental).
6. **Editor Tier 0** when the above has stabilized. Plan at
   `devlogs/followups/mission-editor-tier0.md`.
7. **Adopt Thranduil's ASan harness pattern** when there's a quiet
   slot — he's running it; we're not.
8. **Night-mission rendering** (followup
   `night-mission-rendering.md`). Bigger investigation; deferred
   until lighting pipeline gets attention.

Watching (no action required unless they engage):

- Methuselas pushing his editor branch publicly.
- Thranduil's response on discussion #2.
