# Project state

Living snapshot of current branches, external collaboration, and open
work. Refreshed when something material changes — not on a calendar.

Last update: 2026-04-25 (scope framed as "vanilla plus"; external
collaboration de-prioritized in favor of own-fork work).

## Positioning — "MC2 vanilla plus"

The project's shape is **MechCommander 2 vanilla plus** — the
definitive modern way to play the 2001 retail campaign with quality-of-
life fixes and compatibility work on top. Not a total conversion, not
a reimagining, not a mod platform (that's ThranduilsRing's direction).

Focus is own-fork work on the vanilla experience:

- Font rendering quality (current fork regressed vs retail)
- UI resizing / repositioning for modern resolutions
- Pre-mission briefing map bug fixes
- Mission editor revival so the community can author custom missions
- Crash / stability fixes (FMV pipeline done; more as they surface)
- Installer + distribution polish

External collaboration is **outbound-only** for now: we send PRs to
alariq when he starts responding, and we rebase/integrate with
ThranduilsRing when he pushes his batch or replies on discussion #2.
No active chasing either direction; when they engage, we engage.

## Active branches

- `master` — shipping baseline. Has the FMV playback fixes squashed from
  the retired `fmv-mp4-support` branch (isPlaying gate, `.bik` strip,
  full-screen rect), campaign-dialog dup fix cherry-picked from the
  upstream PR, modding guide (`CUSTOM-CAMPAIGNS.md`), editor Tier 0 plan
  under `devlogs/`, README rewrite, compass fix (`379c8d5`).
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
— agreed to converge our forks. His engine modernization (Tracy
profiler, render contract, upcoming test harness + ASan + crash
handler) + our FMV pipeline + crash fixes. Attribution preserved via
co-author tags. Current discussion thread:
<https://github.com/ThranduilsRing/mc2-opengl-remastered/discussions/2>.

**Current state (2026-04-25):** his last push was 2026-04-22
(`7d6f9bc`). No reply on discussion #2 yet. His "FMV + test harness +
ASan in a few days" note was from an earlier exchange; as of 2026-04-25
that push hasn't landed. We are **holding** non-FMV rebase work until
he pushes — no ping, no preemptive work. Revisit if another week
passes with no activity.

Plan when he pushes:

1. Add his repo as a git remote (`thranduil` → `https://github.com/
   ThranduilsRing/mc2-opengl-remastered.git`).
2. Rebase the **non-FMV work** (currently = `input-fixes` branch) onto
   his `main` so we share an incremental baseline.
3. **Park our FMV work** for later diff-and-integrate against his FMV,
   then do one integration cycle picking the better parts.

**Known collision risk at rebase time:** his 2026-04-18 HUD scene-
split plan will clash with our FMV HUD touchpoints (`controlgui.cpp`,
`forcegroupbar.cpp`, `logistics.cpp`, `missionselectionscreen.cpp`).
Resolve at rebase time, not earlier.

**Open question awaiting his response:** our Inno Setup installer vs.
his upcoming mod-launcher + Exodus-as-mod distribution model. Either
retire ours once his launcher is canonical, or keep it as a "vanilla-
plus-fixes, no mods" entry point.

**On OpenGL → RHI → Vulkan** (his long-term question): yes if Mac is
a real goal (MoltenVK path). RHI as an intermediate layer first,
decouples renderer from backend. Not urgent.

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

1. **Input-fixes stability check.** Only remaining investigation branch
   with in-flight work. The ClipCursor reinforcement has a known
   1-pixel-column residual on one 3-monitor setup; not urgent, likely
   environmental.
2. **Wait for ThranduilsRing's next push.** When it lands, start the
   rebase of non-FMV work onto his `main` per discussion #2.
3. **Merge `input-fixes` to master** when validated.
4. **Editor Tier 0** when the above has stabilized. Plan at
   `devlogs/followups/mission-editor-tier0.md`.
