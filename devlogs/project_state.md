# Project state

Living snapshot of current branches, external collaboration, and open
work. Refreshed when something material changes — not on a calendar.

Last update: 2026-04-25 eve (positioning locked publicly in discussion
#2: vanilla+ with cross-OS reach vs. Thranduil's Windows-first
remaster; 3 of Thranduil's base-game fixes cherry-picked to master).

## Positioning — "MC2 vanilla plus, cross-OS"

The project's shape is **MechCommander 2 vanilla plus** with **cross-
OS reach (Windows, Linux, Mac)** — the definitive modern way to play
the 2001 retail campaign on whatever OS you're on, with quality-of-
life fixes and compatibility work on top. Not a total conversion, not
a reimagining, not a remaster (that's ThranduilsRing's direction —
visual fidelity, Windows-first, ships upscaled data).

Focus is own-fork work on the vanilla experience:

- Font rendering quality (current fork regressed vs retail)
- UI resizing / repositioning for modern resolutions
- Pre-mission briefing map bug fixes
- Mission editor revival so the community can author custom missions
- Crash / stability fixes (FMV pipeline done; more as they surface)
- Installer that patches over a legal retail copy (no asset
  redistribution; license anchor via `Mc2Rel.exe`)
- Cross-OS compat work as it surfaces (Linux already works via
  alariq's port; Mac is future via MoltenVK / RHI)

External collaboration is **bidirectional but bounded**: bug fixes
flow both ways with ThranduilsRing without either fork having to
adopt the other's scope. Outbound-only to alariq for upstream PRs.
No tree convergence; both projects coexist as different surfaces for
different users.

## Active branches

- `master` — shipping baseline. Has the FMV playback fixes squashed from
  the retired `fmv-mp4-support` branch (isPlaying gate, `.bik` strip,
  full-screen rect), campaign-dialog dup fix cherry-picked from the
  upstream PR, modding guide (`CUSTOM-CAMPAIGNS.md`), editor Tier 0 plan
  under `devlogs/`, README rewrite, compass fix (`379c8d5`), and three
  base-game crash fixes cherry-picked from Thranduil's v0.2 with `-x`
  attribution and pushed to origin: `eb7afb2` (Turret teamId guard),
  `bcdbb4b` (ABL code-segment sentinel byte), `4c5de32` (GlobalMap
  pathExistsTable zero-init).
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

Retired (squash-merged into master):

- `compass-fix` — 2026-04-25 as `379c8d5`. Root cause was an alpha-test
  fragment discard in `shaders/gos_tex_vertex.frag` killing soft-alpha
  compass pixels. Investigation trail at
  `devlogs/closed/compass_investigation_2026-04-25.md`.
- `fmv-mp4-support` — 2026-04-24.
- `fmv-mp4-support-backup` — user's personal safety branch, untouched.

## External collaboration

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

## Upstream PRs to alariq

Open on `alariq/mc2`:

| # | Topic | Notes |
|---|---|---|
| [#23](https://github.com/alariq/mc2/pull/23) | UI scaling | Rebased 2026-04-24. Stalled through 2025. |
| [#24](https://github.com/alariq/mc2/pull/24) | Windows build improvements | Rebased on top of #23. Blocked on #23 first. |
| [#26](https://github.com/alariq/mc2/pull/26) | FMV pipeline | Closes #22. |
| [#28](https://github.com/alariq/mc2/pull/28) | Campaign-dialog dup fix | Closes #27, easy yes. |
| [#29](https://github.com/alariq/mc2/pull/29) | Compass alpha-test fix | Opened 2026-04-25, one-line fix. |

Issues: [#22](https://github.com/alariq/mc2/issues/22) (FMVs missing,
user-opened, closed by #26) and
[#27](https://github.com/alariq/mc2/issues/27) (dup bug, closed by #28).

Per-PR status and outreach history in
`devlogs/followups/alariq-pr-waits.md`.

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

1. **Fonts** (in flight). New `lesson_glyph_uv_renderer_contract.md`
   memory landed during the D3F port; finish the renderer-contract
   work and validate.
2. **Cherry-pick `9751e86` (Thranduil's HUD-inverse mouse transform
   fix) onto `input-fixes`** — overlaps with our cursor work; needs a
   real diff before applying.
3. **Verify `AUDIO_U8` fix in `GameOS/gameos/gameos_sound.cpp:297`** —
   action item from imported memory; if missing, port the one-liner.
4. **Merge `input-fixes` to master** when validated (residual
   1-pixel-column on 3-monitor likely environmental).
5. **Editor Tier 0** when the above has stabilized. Plan at
   `devlogs/followups/mission-editor-tier0.md`.
6. **Adopt Thranduil's ASan harness pattern** when there's a quiet
   slot — he's running it; we're not.

Watching (no action required unless they engage):

- Thranduil's response on discussion #2.
- alariq's response on the open PRs (#23/#24/#26/#28/#29).
