# Mission Editor Revival — Tier 0 Plan (2026-04-24)

Internal plan. Not a commitment — sizing and sequencing for the near-term
"get it building again" scope so the work can start cleanly when we're
ready.

## Goal

**Get the original 2001 Microsoft mission editor compiling and running on
VS 2022 + modern MFC, from `MC2_Source_Code/Source/Editor/`, with no
change in feature set from retail.** Success criterion: a Windows 10/11
user can launch `editor.exe`, open a retail mission `.fit`, modify it,
save, and the modified mission loads and plays correctly in our patched
`mc2.exe`.

Explicitly out of scope for Tier 0:
- Linux / cross-platform support (requires Tier 2 — strip MFC).
- High-DPI fixes, x64 native, Windows 11 common-control polish (Tier 1).
- New features, refactors, or code cleanup beyond "make it compile and
  link cleanly."

## What we know going in

From the investigation on 2026-04-24:

- Editor source is at `MC2_Source_Code/Source/Editor/` — 134 `.cpp`/`.h`
  files. This directory is **untracked** in our repo (reference checkout
  of Microsoft's shared-source drop). Before starting work, it needs to
  be either committed, tracked as a git submodule, or moved into a
  tracked location inside our repo so changes can be version-controlled.
- Project files: `Editores.vcproj` (editor resource DLL) and
  `EditorMFC.vcproj` (editor application). Both are Version="8.00"
  (Visual Studio 2005). VS 2022's `devenv /upgrade` handles this cleanly.
- Depends on **MFC** — 287 references across 77 files. MFC is the GUI
  backbone (dialogs, main frame, property panels). Modern MFC ships with
  VS Community (free for non-commercial) and is backward-compatible.
- **No DirectX.** Zero references to `d3d`, `D3DX`, `IDirect3D*`, or
  `ddraw`. The editor renders through GameOS like the main game, so our
  OpenGL path is already available.
- Reads the same `.fit`/`.pak`/`.fst` files the game uses, via the same
  `mclib` IO layer. No separate authoring formats to port.
- Retail ships a compiled `editores.dll` which our fork's
  `full_game/editores.dll` matches byte-for-byte; it's the resource DLL
  the editor app links against at runtime.

## Estimated effort

**2–5 days** of focused work for Tier 0. Most of the time is expected to
be spent on:

- Header / include-path reconciliation when upgrading `.vcproj` → `.vcxproj`.
- MBCS → Unicode conversion if the editor assumed MBCS char strings and
  modern MFC defaults to Unicode.
- Linker resolution for functions the editor calls into the main engine
  (if any — needs verification during phase 2 below).
- Dealing with whatever assertions the MFC message loop or CWnd hierarchy
  raises on first launch.

## Phases

### Phase 1 — Track the source tree (half-day)

Get `MC2_Source_Code/Source/Editor/` into a state where changes can be
version-controlled. Two options, pick one upfront:

- **(a)** Commit the full tree to our repo under an explicit path such
  as `editor/` or `src/editor/`. Pro: simplest; single-repo workflow.
  Con: adds a large body of third-party source to our history.
- **(b)** Keep Microsoft's drop external, but track just our
  modifications as a patch or overlay. Pro: clean separation of
  "Microsoft's original" vs "our changes". Con: two-tree workflow.

**Recommendation: (a).** The editor source is 134 files, ~3–5 MB, static
(Microsoft isn't releasing new drops). One-time commit of Microsoft's
original followed by patch commits on top gives a clean, reviewable
diff history.

Exit criterion: editor source lives under a tracked path in our repo
and `git log` can show incremental changes.

### Phase 2 — Upgrade the project files (half-day)

- Run `devenv /upgrade Editores.vcproj` and
  `devenv /upgrade EditorMFC.vcproj` in a VS 2022 Developer Command
  Prompt. Produces `.vcxproj` siblings.
- Alternatively, hand-roll a CMake setup to match the rest of the repo's
  build system. **Cost**: writing a CMakeLists for 134 files; **benefit**:
  consistency with how the rest of the project builds. Weighing this
  against auto-upgrade — recommend *auto-upgrade first* for Tier 0 to
  minimize novel work; CMake port is a Tier 1 follow-up if it proves
  necessary for ergonomics.
- Target x86 initially (the source was 32-bit in 2005). x64 is a Tier 1
  concern.
- Ensure the output `.exe` and `.dll` file names match retail
  (`editor.exe`, `editores.dll`) so the runtime side is drop-in.

Exit criterion: `Build → Build Solution` from VS 2022 produces object
files for every translation unit. Link errors are fine at this stage;
compile-clean is the bar.

### Phase 3 — Resolve compile + link errors (1–2 days)

Expected failure modes in rough likelihood order:

- **MFC header paths.** `afxwin.h`, `afxcmn.h`, etc. The upgrade usually
  wires these correctly. If not, add `$(VC_IncludePath);$(WindowsSDK_IncludePath)`
  to the project's include paths.
- **MBCS vs Unicode.** The 2005 project likely targeted MBCS. Modern MFC
  defaults to Unicode. Set `Character Set → Use Multi-Byte Character Set`
  in project properties for Tier 0; Unicode migration is Tier 1.
- **Deprecated C-runtime functions.** `_stricmp`, `strcpy` without length
  bounds, etc. — MSVC now warns/errors in strict mode. Project is old
  enough that these should be OK at default warning level, but add
  `_CRT_SECURE_NO_WARNINGS` to the preprocessor defines if needed.
- **Missing externs.** The editor calls into GameOS and `mclib`
  functions. If the source tree is built standalone without linking
  against those libraries, many symbols will be unresolved. The project
  file likely references library dependencies explicitly — if those
  paths are stale, update them to point at our repo's current lib output
  (`build64/out/mclib/*.lib` etc.).
- **Direct3D 8/9 SDK headers.** We confirmed no `d3d`/`D3DX` usage in
  the editor, so this shouldn't apply. But flag anything unexpected.
- **Old-style `BOOL` / `LPCTSTR`** — just Windows types, should still work.

Exit criterion: `editor.exe` and `editores.dll` produced with no errors.
Warnings acceptable.

### Phase 4 — First-launch smoke test (half-day)

- Drop `editor.exe` next to `mc2.exe` in an install dir that has the full
  data tree.
- Launch. Expect either: it opens, or it crashes/assertions quickly.
- If it opens:
    - **Validation A:** File → Open → pick a retail mission file (e.g.
      `mc2_01.fit`). Does it load? Does anything render?
    - **Validation B:** Modify something trivial (move a waypoint, change
      an objective's text) → Save → As → `mc2_01_test.fit`.
    - **Validation C:** In `mc2.exe`, launch the modified mission either
      via a campaign `.fit` pointing at it or via the test-mission
      command-line path. Does it load and behave correctly?
- If it crashes at launch, first moves:
    - Run under the VS 2022 debugger; capture the first assertion or
      exception.
    - Common culprits: missing `editores.dll` at runtime, uninitialized
      `AfxGetApp()` pointer, bad resource string IDs.

Exit criterion: Validations A, B, C all pass.

### Phase 5 — Documentation + release (half-day)

- Commit the upgraded `.vcxproj` files (or CMakeLists.txt if we went that
  route) alongside Microsoft's source.
- Add an `EDITOR.md` at repo root with build steps and known limitations.
- Update `README.md` "In progress / on deck" — remove mission editor
  revival, add it to the "Completed and verified" list.
- Update `claude.md` pending-follow-ups list — move the editor entry to
  resolved.
- Optionally: ship the built `editor.exe` + `editores.dll` alongside the
  installer as an optional component (gated behind a wizard checkbox).

Exit criterion: a clone of the repo + VS 2022 = one command to produce a
working editor binary.

## Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| MFC paid-only in VS 2022 | Low | Hard blocker | MFC still ships free in VS Community Edition as of 2026. Confirm before starting. |
| Hard linker dependency on retired Microsoft libs | Medium | Blocker | If the editor calls into `dsound.lib`, `dinput.lib`, etc. — those still ship in the current Windows SDK. |
| 2005-era C-runtime assumptions break under VS 2022's CRT | Medium | Time cost | Add `_CRT_SECURE_NO_WARNINGS`, fall back to legacy CRT if needed via `/D_CRT_INSECURE_DEPRECATE`. |
| MFC resource IDs in `.rc` collide with main app | Low | Runtime crash | Editor's `.rc` is in a separate DLL (`editores.dll`); collisions would be self-contained. |
| Editor asserts on 1920×1080 windowed mode | Medium | UX issue | Acceptable for Tier 0 — work around with smaller window size; Tier 1 fixes. |
| Save files produced by the editor load, but one-off fields we don't know about corrupt something mid-game | Low-Medium | Hard to debug | Diff a round-tripped `.fit` against the original after a no-op save. If fields are reordered/dropped, triage then. |

## Definition of done

A developer with VS 2022 installed, cloning the repo fresh and opening
`editor.vcxproj` (or equivalent), can:

1. `Build → Build Solution` → `editor.exe` + `editores.dll` produced.
2. Copy both into a working MC2 install alongside `mc2.exe`.
3. Run `editor.exe` → it opens.
4. Open `mc2_01.fit` → mission renders.
5. Save a modified copy → modified mission runs correctly in `mc2.exe`.

With all five passing, Tier 0 is complete. Tier 1 (x64, high-DPI, Win11
polish) becomes the next chunk of work.

## Sequencing considerations

- Tier 0 doesn't need to block anything else in the project. It's
  Windows-only and self-contained. A reasonable position for it in the
  roadmap is after the next batch of `mc2.exe` stability fixes —
  shipping a stable v0.2 of the patch installer should take priority
  over editor revival for public launch.
- Community demand for the editor is steady (referenced in Reddit posts
  and several GitHub issues on the fork ecosystem). Landing Tier 0 is a
  high-visibility milestone that'd get strong reception.
- If we end up collaborating with ThranduilsRing on a merged project,
  this plan carries over unchanged — the editor doesn't overlap with
  their renderer work. Worth surfacing as a workstream they don't need
  to co-own.
