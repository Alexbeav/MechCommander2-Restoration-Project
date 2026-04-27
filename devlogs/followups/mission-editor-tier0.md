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

Community is asking about it. Initial survey 2026-04-24, deeper dive
2026-04-26 after Methuselas reported a working editor on his fork in
discussion under PR alariq/mc2#26.

- `.vcproj` files are Version="8.00" (VS 2005) — VS 2022 `devenv /upgrade`
  handles this cleanly.
- **No DirectX usage in the editor source.** Two grep hits are a
  commented-out `//#include "d3dfont.h"` in `Editor.cpp:22` and a config
  string `"CDsoundPath"` (false positive). The `.vcproj` does add
  `..\..\DirectX8\lib` to its lib path, but that's an artifact of the
  era's gameos build — irrelevant for our OpenGL path.
- Uses the same `.fit`/`.pak`/`.fst` formats via `mclib`, not its own
  authoring formats.
- MFC depth (recounted with broader pattern): 871 refs across 83 of
  ~139 files. MFC is the GUI backbone, so Tier 2 cross-platform port is
  genuinely months. Modern MFC ships free in VS Community and is
  backward-compatible, so Tier 0/1 are more tractable than the raw file
  count suggests.

### Found in the deeper dive (2026-04-26)

These weren't in the original survey and materially change Tier 0 effort.

- **Editor is NOT in `MechCmd2.sln`.** The master solution lists 10
  projects (game, GUI, MCLib, MLR, Stuff, gosFX, mc2res, ablt, aseconv,
  Viewer); `EditorMFC.vcproj` and `Editores.vcproj` are absent. The
  editor was always built standalone — there's no "Build Solution" path
  that produces it.
- **`mfcplatform.lib` source was never released.** The editor links
  against `..\gameos\lib\$(Cfg)\mfcplatform.lib`. The header
  (`MC2_Source_Code/Source/GameOS/include/MFCPlatform.hpp`, identical to
  our `GameOS/include/mfcplatform.hpp`) declares 6 stdcall entry points:
  `InitGameOS`, `RunGameOSLogic`, `ExitGameOS`, `GameOSWinProc`,
  `ProcessException`, `InitExceptionHandler`. **No implementation exists
  in MS's drop or our tree.** It was a separately-built lib MS kept
  internal. We'd need to recreate it — equivalent to splitting the
  contents of our `GameOS/gameos/gameosmain.cpp:225 main()` into three
  callable lifecycle functions. The existing `DISABLE_GAMEOS_MAIN` guard
  in that file suggests prior anticipation of this split.
- **SDL/HWND ownership mismatch.** Our gameos creates its window via
  `graphics::create_window("mc2", w, h)` — SDL2 owns the HWND. MFC
  brings its own HWND from the editor's main frame and expects gameos
  to render into it. Either wrap the MFC HWND with
  `SDL_CreateWindowFrom(hwnd)` or refactor `gos_CreateRenderer` to
  accept an external HWND. **This is the genuine technical risk; the
  prior plan did not flag it.**
- **Bitness mismatch.** `.vcproj` is `TargetMachine="1"` (x86). Our
  build is x64-only. Either produce x86 builds of mclib/gameos/gosfx/
  mlr/stuff/zlib (mechanical but adds a build matrix), or port the
  editor to x64 (more invasive — `int`/`LONG_PTR` audits, MFC handle
  width). x64 is cleaner long-term but lifts effort.
- **`_ARMOR` and `LAB_ONLY` already match our debug build.** The
  editor's preprocessor flags are
  `WIN32;_DEBUG;_WINDOWS;LAB_ONLY;_ARMOR;WINVER=0x0501`. WINVER bumps to
  0x0A00 are trivial.

## Effort tiers (revised 2026-04-26)

- **Tier 0 — revive as-is on VS 2022 + MFC.** ~~2-5 days~~ **5-10 days**
  with the `mfcplatform.lib` recreation, SDL-on-HWND glue, and bitness
  decision. Windows-only.
- **Tier 1 — Tier 0 + Win11/x64/high-DPI modernization.** 2-4 weeks.
- **Tier 2 — port UI to cross-platform** (Qt or Dear ImGui). Months.

## Methuselas's parallel work

Per his comment under alariq/mc2#26 (2026-04-26): ~2 days of work,
already has menus working, campaign editor done, mission editor done,
textures rendering, screenshot at 1920×1050 OpenGL. Outstanding on his
side: POSIX-name warnings, hardcoded menus → Fit/CSV, map sizes >180.
**Adopting his branch would skip all the Tier 0 work above** if the
integration cost (his renderer divergence, his `mfcplatform`
equivalent, x86/x64 stance) is lower than building from scratch.
Worth a code-level look at his branch before committing to either path.

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
