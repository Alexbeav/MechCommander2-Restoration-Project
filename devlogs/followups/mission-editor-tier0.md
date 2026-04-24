# Mission editor revival — Tier 0

Opened 2026-04-24. Status: plan only; work not started. Waiting on
input-fixes stabilization before picking this up.

Microsoft shipped the mission editor source with the 2001 shared-source
drop. Full tree lives at `MC2_Source_Code/Source/Editor/` (134
`.cpp/.h` files, untracked locally — reference checkout). `.vcproj`
files in that dir target VS 2002-2008 and use MFC. Retail shipped a
compiled `editores.dll` (resource DLL) and an `EditorMFC` application;
our `full_game/editores.dll` is that retail DLL,
`full_game/EDITOR/EDITOR.RTF` is the 542 KB user manual.

No fork currently builds the editor. alariq skipped it because MFC
makes it Windows-only; ThranduilsRing hasn't touched it.

Community is asking about it. Investigated 2026-04-24:

- `.vcproj` files are Version="8.00" (VS 2005) — VS 2022 `devenv /upgrade`
  handles this cleanly.
- **No DirectX in the Editor tree.** Renders through GameOS like the main
  game; our OpenGL path is already available.
- Uses the same `.fit`/`.pak`/`.fst` formats via `mclib`, not its own
  authoring formats.
- MFC depth: 287 refs across 77 files (out of ~134). MFC is the GUI
  backbone, so Tier 2 cross-platform port is genuinely months. Modern MFC
  ships free in VS Community and is backward-compatible, so Tier 0/1 are
  more tractable than the raw file count suggests.

## Effort tiers

- **Tier 0 — revive as-is on VS 2022 + MFC.** 2-5 days. Windows-only.
- **Tier 1 — Tier 0 + Win11/x64/high-DPI modernization.** 2-4 weeks.
- **Tier 2 — port UI to cross-platform** (Qt or Dear ImGui). Months.

Full phase plan in `devlogs/mission_editor_tier0_plan.md`.

## Branch

`editor-tier0` exists as an empty placeholder off master. Step 0 is
unignore + commit `MC2_Source_Code/` (currently in `.gitignore:97`).

## Custom videos work without the editor

Separately verified during the same investigation — video references in
mission/campaign `.fit` files are `EString` path strings, not a
hardcoded enum:

- `logisticsmissioninfo.h:158-160` — `EString videoFileName;` and
  `EString bigVideoName;` on the MissionGroup struct.
- `logisticsmissioninfo.cpp:162-168` — load path accepts
  `PreVideo = "..."` / `VideoOverride = "..."` as free strings.
- `missionselectionscreen.cpp:240` — path already built with `.mp4`
  extension.

So a hand-authored test campaign is possible today with zero code
changes: clone existing `.fit` files, edit the video-field strings to
reference custom `.mp4` filenames, drop the `.mp4`s into
`full_game/data/Movies/`. That's the low-cost validation path before
committing to editor revival. Documented for modders in
`CUSTOM-CAMPAIGNS.md`.
