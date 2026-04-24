# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### IMPORTANT: Testing Protocol
**DO NOT run the game.** The user will run `full_game/mc2.exe` themselves after each build to verify changes. Only build the project — never attempt to execute mc2.exe.

The `mc2` CMake target's POST_BUILD step automatically copies `build64/Release/mc2.exe` to `full_game/mc2.exe` so the user can re-test immediately after each build. Do not strip that POST_BUILD command, and do not invoke a separate copy yourself — the build handles it.

### Prerequisites
- **Visual Studio 2022** with C++ build tools
- **CMake** (3.10 or higher)
- **Windows SDK** 10.0.22621.0 or similar
- **Git** (for cloning the repository)

### Windows Build Process (TESTED & WORKING)

**Step 1: Extract 3rdparty Dependencies**
The repository includes a `3rdparty.zip` file that contains all necessary libraries. This needs to be extracted to a temporary `3rdparty/` folder (this folder is not committed to git):
```
mc2/
├── 3rdparty.zip          (committed)
├── 3rdparty/             (temporary, extract from zip)
│   ├── cmake/
│   ├── include/
│   └── lib/
│       ├── x64/
│       └── x86/
```

**Step 2: Extract Dependencies (if not using setup script)**
```bash
# Extract 3rdparty.zip to create temporary 3rdparty/ folder
powershell -Command "Expand-Archive -Path '3rdparty.zip' -DestinationPath '.' -Force"
```

**Step 3: Main Application Build**
```bash
# Navigate to project root
cd G:/games source code/games/mc2

# Create build directory
mkdir build64
cd build64

# Configure with CMake - use ABSOLUTE path to 3rdparty
cmake.exe -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="G:/games source code/games/mc2/3rdparty" -DCMAKE_LIBRARY_ARCHITECTURE=x64 ..

# Build the project
cmake --build . --config Release
```

**Step 4: Resource DLL Build**
```bash
# Navigate to res directory
cd G:/games source code/games/mc2/res

# Create build directory
mkdir build64
cd build64

# Configure resource DLL build
cmake.exe -G "Visual Studio 17 2022" -DCMAKE_LIBRARY_ARCHITECTURE=x64 ..

# Build the DLL
cmake --build . --config Release
```

**Step 5: Prepare String Resources**
```bash
# Copy the pre-generated string resources to res directory
copy "test_scripts\res_conv\strings.res.cpp" "res\"
copy "test_scripts\res_conv\strings.res.h" "res\"
```

### Build Outputs
After successful build, you'll find:
- **Main executable**: `build64/Release/mc2.exe`
- **Resource DLL**: `build64/out/res/Release/mc2res_64.dll`
- **Data tools**: `build64/out/data_tools/Release/` (aseconv.exe, makefst.exe, makersp.exe, pak.exe)
- **Text tool**: `build64/out/text_tool/Release/text_tool.exe`
- **Viewer**: `build64/out/Viewer/Release/viewer.exe`

### Build Notes
- The build generates many warnings about type conversions (size_t to int, etc.) but these are expected for this legacy codebase
- The CMake configuration automatically detects x64 vs x86 architecture
- SDL2, GLEW, and zlib are all properly detected from the 3rdparty folder
- The resource DLL is built as mc2res_64.dll for 64-bit builds

### Data Building
Game data is built separately using the mc2srcdata repository. Use the tools from the build output:
- Copy all tools from `build64/out/data_tools/Release/` to `mc2srcdata/build_scripts/`
- Copy required DLLs from `3rdparty/lib/x64/` to the same folder
- Run `make all` in the build_scripts folder

### Quick Start Scripts
Two convenience scripts are provided:
- **`setup_build_environment.bat`** - Extracts 3rdparty.zip and checks build tools
- **`build_windows.bat`** - Complete build process for Windows

**Note:** The scripts automatically extract `3rdparty.zip` to a temporary `3rdparty/` folder. This folder is not committed to git and should be treated as a build artifact. The original `3rdparty.zip` remains in the repository for convenience.

### Common Issues and Solutions

**CMake can't find libraries:**
- Ensure you're using the ABSOLUTE path to the 3rdparty folder
- Verify that 3rdparty.zip was extracted properly
- Check that you're using the correct architecture (x64)

**Visual Studio not found:**
- Install Visual Studio 2022 with C++ Desktop Development workload
- Run builds from Visual Studio Developer Command Prompt
- Ensure Windows SDK is installed

**Build warnings:**
- The build generates many warnings about type conversions (size_t to int, etc.)
- These are expected for this legacy 2001 codebase and don't affect functionality

**Missing DLLs at runtime:**
- Copy all DLLs from `3rdparty/lib/x64/` to your executable directory
- Required DLLs: SDL2.dll, SDL2_mixer.dll, SDL2_ttf.dll, glew32.dll, zlib1.dll

**String resource errors:**
- Copy strings.res.cpp and strings.res.h from `test_scripts/res_conv/` to `res/`
- These are pre-generated and don't need to be rebuilt

## Architecture Overview

### Core Architecture
This is MechCommander 2, a real-time strategy game engine with these major components:

**GameOS Layer** (`GameOS/`):
- Cross-platform abstraction layer for graphics, input, sound, and file I/O
- Contains the main game loop and window management
- Handles OpenGL rendering, SDL integration

**MCLib** (`mclib/`):
- Core game engine library with utilities and low-level systems
- Contains key subsystems:
  - **MLR** (`mclib/mlr/`): 3D rendering and mesh system
  - **gosfx** (`mclib/gosfx/`): Special effects and particle system  
  - **stuff** (`mclib/stuff/`): Math utilities (vectors, matrices, etc.)
- File I/O, memory management, terrain rendering

**GUI System** (`gui/`):
- Custom GUI framework for game menus and interfaces
- Animation system, buttons, listboxes, text editing
- Logistics screens for mech configuration

**Game Logic** (`code/`):
- Core game entities: mechs, buildings, weapons, terrain objects
- Mission system, AI, physics, collision detection
- Campaign progression, pilot management
- Multiplayer networking support

### Key Systems

**Rendering Pipeline:**
- Uses OpenGL with custom shader system (`shaders/`)
- Terrain rendering with texture management
- 3D model loading and animation
- Particle effects and special effects

**Game Objects:**
- Hierarchical object system with base GameObject class
- Specialized types: Mech, Building, Vehicle, Turret, etc.
- Component-based approach for weapons, sensors, etc.

**Mission System:**
- ABL (A Behavior Language) scripting for AI and missions
- Trigger system for mission events
- Save/load game state management

**Audio System:**
- Sound effects, music, and voice-over support
- 3D positional audio (partially implemented)

### Build Configuration
- Debug builds use `-D_ARMOR -D_DEBUG -DBUGLOG -DLAB_ONLY` flags
- Cross-platform compilation with Windows and Linux support
- Uses CMake with separate configurations for x86/x64

### Testing
No automated test framework is present. Testing is manual gameplay validation.

### Data Files
Game uses custom file formats (.fst, .pak, .tga) built from source assets in separate mc2srcdata repository.

## MP4 Video Playback

**Canonical source:** `FMV_DESIGN.md` at the repo root is authoritative for the in-progress
migration to the new `MC2Movie` (gos-texture) architecture. `FMV_STATUS.md` contains the
pre-migration state plus a migration-progress block at the top.

### Current architecture (mid-migration)
Two video classes coexist; both are linked into `mc2.exe`:

- **`MC2Movie`** (`code/mc2movie.{h,cpp}`) — new design. Decodes FFmpeg → uploads into a
  gos-owned texture (`gos_NewEmptyTexture` + `glTexSubImage2D` via `gos_GetTextureGLId`).
  Has **no `render()` method**; callers draw via `gos_SetRenderState(gos_State_Texture, …)`
  + `gos_DrawQuads(v, 4)`. **Live at:** mission selection (`missionselectionscreen.cpp`),
  in-mission cinematics (`controlgui.cpp`).
- **`MP4Player`** (`code/mp4player.{h,cpp}`) — legacy direct-GL path. Manages its own
  `GLuint` texture, viewport, scissor, and full GL state save/restore in `render()`.
  **Live at:** credits (`logistics.cpp`), intro (`mainmenu.cpp`), pilot portraits
  (`forcegroupbar.cpp` / `mechicon.cpp`), full-screen cinematics
  (`logistics.cpp::playFullScreenVideo`). To be removed in step 8 of `FMV_DESIGN.md` §10.

### Pilot-portrait videos — fixed 2026-04-23

Two bugs stacked, both resolved:

1. **sws_scale SIMD tail overrun** (commit `575afc6`). `sws_scale` with `SWS_BILINEAR`
   overruns the RGBA destination buffer by up to ~32 bytes on SIMD-unfriendly widths
   (76-wide pilot-portrait clips). `av_image_get_buffer_size(align=1)` allocates exactly
   W*H*4, so the overrun corrupts heap metadata and raises `STATUS_HEAP_CORRUPTION` on
   the next heap op (usually inside `SDL_GL_SwapWindow`). Fixed by keeping `align=1` for
   packed downstream layout, but allocating `rgbBuf` and `pendingData` with a 64-byte tail
   guard across `mp4player.cpp`, `mc2movie.cpp`, and `clean_mp4_player.cpp`. MC2Movie also
   got pixel-store sanitation (`glPixelStorei(GL_UNPACK_{ALIGNMENT,ROW_LENGTH})`) before
   `glTexSubImage2D` so stale row-length state can't skew frames.

2. **`ForceGroupIcon::bMovie` UAF** (commit `787cbb0`). `bMovie` was a `static` shared
   across all icon instances with four delete paths licensed to free it. Demoted to a
   per-instance member with the invariant "at most one icon holds bMovie non-null at a
   time," owned structurally by `ForceGroupBar::setPilotVideo()`. Defensive pre-allocation
   walk catches invariant violations at the call site.

Verified end-to-end: MSFT intro + CINEMA1-5, pre-mission briefing, in-mission cinematics,
pilot acknowledgement videos. The `prefs.pilotVideos = false` override has been removed.

**Follow-up tracked separately:** `forcegroupbar.cpp:502` still initializes the pilot
video with `loop=true`. Pilot acks are one-shot voice-overs; the original intent is
unclear. Revisit as a non-crash behavioral tweak.

Decode/audio internals are identical between the two classes (cloned, not shared). The
differences are texture ownership and rendering responsibility.

### Shared properties (both classes)
- **Audio-master clock** with wall-clock fallback; video frames are **PTS-gated**.
- **Separate packet queues** (ffplay/VLC-style): audio and video are decoupled and consumed
  at their own rate.
- **Standalone reference** implementation: `code/clean_mp4_player.cpp`, build target
  `clean_mp4_player`. Uses the `MP4Player`-style decode pipeline; validate decode bugs there
  before attributing them to game integration.

### Do not reintroduce
The following were deliberately removed and live in `archive/fmv-dead-code/`:
- The 2025 `MC2Movie` wrapper class (different design from the current `MC2Movie`) —
  archived under `archive/fmv-dead-code/code/mc2movie.{cpp,h}`.
- `PlayMP4()` wrapper function — replaced by direct player use.
- `video_position_fix.{cpp,h}`, `frame_rate_fix.{cpp,h}` — band-aids on a broken pipeline.
- The `mp4_standalone` and `fmv_test_runner` CMake targets.

Historical session logs that previously lived in this section have been moved to
`archive/fmv-dead-code/docs/CLAUDE_md_fmv_history.md`. They describe the abandoned
architecture and should not be followed.

## Pending follow-ups

### Clamp cursor to game window on borderless (2026-04-24)

Cursor can escape the game window onto adjacent monitors on multi-monitor
setups because nothing clips it. We already hide the OS cursor while the
window is active (`GameOS/gameos/gos_render.cpp`, `SDL_ShowCursor(SDL_DISABLE)`).
Need to add `SDL_SetWindowGrab(window, SDL_TRUE)` (or the newer
`SDL_SetWindowMouseGrab`) in the same code path so the cursor is confined
to the window bounds while focused. Release on focus-lost so Alt-Tab works
naturally. ~5-line change.

### Mission editor revival (2026-04-24)

Microsoft shipped the mission editor source with the 2001 shared-source drop.
Full tree lives at `MC2_Source_Code/Source/Editor/` (134 `.cpp/.h` files,
untracked locally — reference checkout). `.vcproj` files in that dir target
VS 2002-2008 and use MFC. Retail shipped a compiled `editores.dll` (resource
DLL) and an `EditorMFC` application; our `full_game/editores.dll` is that
retail DLL, `full_game/EDITOR/EDITOR.RTF` is the 542 KB user manual.

No fork currently builds the editor. alariq skipped it because MFC makes it
Windows-only; ThranduilsRing hasn't touched it.

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

Effort tiers:

- **Tier 0 — revive as-is on VS 2022 + MFC.** 2-5 days. Windows-only.
- **Tier 1 — Tier 0 + Win11/x64/high-DPI modernization.** 2-4 weeks.
- **Tier 2 — port UI to cross-platform** (Qt or Dear ImGui). Months.

**Custom videos work without the editor** — separately verified during the
same investigation. Video references in mission/campaign `.fit` files are
`EString` path strings, not a hardcoded enum:

- `logisticsmissioninfo.h:158-160` — `EString videoFileName;` and
  `EString bigVideoName;` on the MissionGroup struct.
- `logisticsmissioninfo.cpp:162-168` — load path accepts
  `PreVideo = "..."` / `VideoOverride = "..."` as free strings.
- `missionselectionscreen.cpp:240` — path already built with `.mp4`
  extension.

So a hand-authored test campaign is possible today with zero code changes:
clone existing `.fit` files, edit the video-field strings to reference
custom `.mp4` filenames, drop the `.mp4`s into `full_game/data/Movies/`.
That's the low-cost validation path before committing to editor revival.

### Release installer (Inno Setup) — nearly complete (2026-04-24)

Under `release/` (gitignored). `mc2-mp4-patch.iss` handles wizard, validation,
file copy, transcode, uninstall. Tested end-to-end with wipe/restore loop.

**Resolved 2026-04-25 — installer now ships `shaders/`.** The payload list was
missing `full_game\shaders\*`, so a fresh retail install booted without GLSL
shader sources. `gosRenderMaterial::load()` opens them with plain `fopen`
relative to CWD (`GameOS/gameos/utils/stream.cpp:78`), bypassing the fastfile
fallback in `mclib/file.cpp`. In release builds `gosASSERT` compiles out, so the
null material is stored and kills the process on the first textured draw —
matching the 7-10 second intro death with `0xC0000005 / RAX=0` at
`mc2.exe+0x1f78b5`. Confirmed by copying `full_game\shaders\` into
`F:\Games\MechCommander2\` and rerunning: game boots clean. Installer fix is
one `[Files]` line plus an `[UninstallDelete]` entry.

Credit: external evaluator flagged the missing `shaders/` in the payload and
pointed out that the original debug plan (symbolize with `full_game/mc2.pdb`)
was unreliable — the PDB is from 2025-05-09 while the shipped `mc2.exe` is
from 2026-04-23, so the symbol names were not trustworthy.

Three small follow-ups remain before public release:

- **Bundle a static ffmpeg.exe** under `release/tools/` (~50 MB gyan.dev
  essentials build) so the installer doesn't require PATH ffmpeg.
- **Revert compression to `lzma2/max` + `SolidCompression=yes`** for public
  release. Currently set to `none` for fast iteration builds.
- **Fix Inno deprecation warning**: change `ArchitecturesInstallIn64BitMode=x64`
  to `x64compatible`.

Not blocking for development, but required before shipping.

### Auto-detect display resolution on first launch (2026-04-24)

The shipped `options.cfg` hardcodes `ResolutionX = 1920` / `ResolutionY = 1080`.
On any monitor that isn't exactly that resolution, the game launches at the
hardcoded size rather than matching the user's display. Desired behavior: on
first launch (no valid options.cfg, or a sentinel value), query the current
primary display resolution via SDL (`SDL_GetCurrentDisplayMode` returns the
width/height of the active desktop mode) and write those values to options.cfg.
Subsequent launches read whatever the user last saved — the options menu still
lets them change it.

Likely touch point: wherever options.cfg is first loaded / initialized
(search `ResolutionX` in `code/` and `gui/` — a `readIdLong("ResolutionX", ...)`
path is the natural hook). Write the detected value only when the key is
missing, so user-set values are preserved.

### Distribution strategy and license anchor (discovered 2026-04-24)

Discovered while building the MP4 FMV release bundle: alariq's fork is **not** a patch
over retail — it's a full replacement. Retail `.fst` archives differ in both size and
content from the fork's; retail has no `data/campaign/campaign.fit` at all (confirmed
by string-searching every retail `.fst`); the fork's data is produced from the separate
`mc2srcdata` repo per `BUILD-WIN.md` and is not derivable from retail files.

Legal constraint: we do **not** want to publish a GitHub distribution that includes the
game's data/assets, even though MC2 is abandonware. Pattern to follow: Falcon BMS's
"bring your own Falcon 4" approach — distribution includes patched binary + our data,
but requires the user to supply a retail `Mc2Rel.exe` as a license anchor.

**Current state (as of this entry):**
- `release/Install-Patch.bat` enforces the anchor at install time by refusing to run
  unless `Mc2Rel.exe` is present in the target directory.
- `mc2.exe` itself does **not** check for `Mc2Rel.exe` at startup. An attacker can
  bypass the installer's check by creating an empty file of that name.

**TODO — runtime anchor check in `mc2.exe`:**
Add an early-init check (likely in `GameOS/gameosmain.cpp` before SDL init) that calls
`fileExists("Mc2Rel.exe")` and aborts with a MessageBox if missing. ~15-20 lines.
Makes the anchor harder to strip without actually patching the binary. Not DRM — just
a formal declaration of the ownership requirement.

**TODO — lean distribution pass:**
Initial v1 bundle ships the fork's `data/` tree verbatim (~1.9 GB). Most of that is
loose unpacked files that duplicate content already packed inside `.fst` archives.
`mclib/file.cpp:246-254` (`File::open`) tries loose first, then falls back to
`FastFileFind()` in registered fastfiles. So if we strip the loose duplicates and ship
only the `.fst` archives plus files that aren't packed (at minimum:
`data/campaign/campaign.fit`), the binary should boot identically from `.fst`. Expected
size: 400-600 MB instead of 2 GB.

Method: iterative boot-retry. Ship minimum (`.fst`s + obvious non-packed like
`campaign/`), read `mc2_stdout.log` after each failed launch, add back only what it
couldn't find, repeat until clean boot. 3-5 cycles probably. Brittle against future
fork changes but good for one-shot v1 size reduction.

Leaner distribution also reinforces the license-anchor story: smaller the data blob,
the more the value-add lives in the binary the user already owns (`Mc2Rel.exe` as
evidence of entitlement to the asset set our patch extends).

### Font rendering quality regression (discovered 2026-04-24)

The current build renders the in-game UI font noticeably worse than retail — hard to
read at normal resolutions. Root cause surfaced while wiring up the MP4 FMV patch for
a clean retail install: our build hardcodes `.bmp` + `.glyph` as the font format
(`GameOS/gameos/gameos_graphics.cpp:2190-2191`, consumed by `gosFont::load`), but
retail ships `.d3f` + `.tga`. The `full_game/assets/graphics/*.{bmp,glyph}` files that
make the game boot are a pre-converted set inherited from an upstream fork; they look
worse than the retail `.tga` glyph atlases sitting alongside them.

**Investigation starting points:**
- `GameOS/gameos/gos_font.cpp:10-40` — `gos_load_glyphs` reads the `.glyph` sidecar
  (num_glyphs, start_glyph, max_advance, font_ascent, font_line_skip, per-glyph
  metrics). That's the metrics side.
- `gosFont::load` at `gameos_graphics.cpp:2185` — pairs the `.glyph` metrics with a
  `.bmp` atlas loaded via `gosTexture`.
- Retail `.tga` + `.d3f` pair lives untouched in every retail install
  (`assets/graphics/arial8.tga`, `arial8.d3f`, etc.). The `.d3f` is the original
  bitmap-font container; see `MC2_Source_Code/Source/GameOS/include/Font3D.hpp` for
  the original structure.

**Likely fix (not verified):** write a small one-shot tool that reads a retail
`.tga`+`.d3f` pair and emits a matching `.bmp`+`.glyph` pair preserving the retail
pixel data and metrics. That should give retail-identical font quality against the
current binary without touching engine code. ~50-100 lines of C++ leaning on the
existing gosTexture code.

Alternative: patch the engine to load `.d3f`+`.tga` directly and skip the intermediate
format. More work, and risks regressing whatever motivated the original conversion.

### Upstream PRs awaiting alariq (as of 2026-04-24)

Two PRs open on `alariq/mc2` from our fork, both stalled waiting on the maintainer:

- **[#23 UI scaling](https://github.com/alariq/mc2/pull/23)** — opened 2025-04-04. Three
  rounds of review; our last update 2025-09-11 ("Implemented your comments @alariq"); no
  response since. Mergeable status is `UNKNOWN` on GitHub, likely a stale conflict.
- **[#24 Windows build improvements](https://github.com/alariq/mc2/pull/24)** — opened
  2025-07-15. Only copilot review so far; no alariq activity.

alariq pushed 4 commits to master on 2026-04-23 (`8dd96f8`, `414cf38`, `9740e68`,
`53c5484`) but did not act on either PR. None of his changes overlap with our open PRs
or the in-flight `fmv-mp4-support` branch.

Once the MP4 FMV patch ships, **rebase both PR branches onto current `upstream/master`
and force-push** to recompute mergeability and surface them in his inbox as "updated".
Only then add a light ping comment on #23 anchoring off the existing conversation.
Don't ping #24 cold — no prior conversation to anchor, reads pushy.
