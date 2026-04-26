# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

### Testing protocol — critical

**DO NOT run the game.** The user runs `full_game/mc2.exe` themselves
after each build to verify changes. Only build — never execute
`mc2.exe`.

The `mc2` CMake target's POST_BUILD step automatically copies
`build64/Release/mc2.exe` to `full_game/mc2.exe`. Do not strip the
POST_BUILD command and do not invoke a separate copy — the build
handles it.

### Standard command

```
cmake --build build64 --config Release --target mc2
```

First-time setup (extract `3rdparty.zip`, run CMake configure, etc.):
see `BUILD-WIN.md` for the full instructions. Quick-start scripts
`setup_build_environment.bat` and `build_windows.bat` handle the
whole dance.

### Build outputs

- Main executable: `build64/Release/mc2.exe` (auto-copied to
  `full_game/mc2.exe` — see above)
- Resource DLL: `build64/out/res/Release/mc2res_64.dll`
- Data tools: `build64/out/data_tools/Release/` (aseconv, makefst,
  makersp, pak)
- Text tool: `build64/out/text_tool/Release/text_tool.exe`
- Viewer: `build64/out/Viewer/Release/viewer.exe`
- Standalone FMV reference player:
  `--target clean_mp4_player` (not `mp4_standalone` — removed)

Expect many `size_t→int` truncation warnings; legacy codebase, not
load-bearing.

### Data

Game data builds from the separate `mc2srcdata` repo. Copy
`build64/out/data_tools/Release/*` + `3rdparty/lib/x64/*.dll` to
`mc2srcdata/build_scripts/` and `make all`.

### Debug build flags

`-D_ARMOR -D_DEBUG -DBUGLOG -DLAB_ONLY`. Release builds compile out
`gosASSERT` — see `lesson_packaging_fopen_paths.md` for how that
interacts with missing-file crashes.

### Log file

`full_game/mc2_stdout.log`. stdout+stderr redirected there by
`gameosmain.cpp` (`_IONBF`, `"w"` mode — truncates each run).

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

No automated test framework. Testing is manual gameplay validation.
Cross-platform compilation with Windows and Linux support.

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

### Pilot-portrait videos

Fixed 2026-04-23 (commits `575afc6`, `787cbb0`). Two bugs stacked: an
`sws_scale` SIMD tail overrun (memory lesson:
`lesson_sws_scale_simd_overrun.md`) and a UAF on `ForceGroupIcon::bMovie`
(memory: `project_pilot_video_crash.md`). See commit messages for full
detail. **Do not reintroduce:** `bMovie` must be per-instance, not
`static`; all `sws_scale` RGBA destination buffers need a 64-byte tail
guard even with `av_image_get_buffer_size(align=1)`.

**Open follow-up:** `forcegroupbar.cpp:502` still initializes pilot
video with `loop=true`. Pilot acks are one-shot voice-overs; the
original intent is unclear. Non-crash behavioral tweak, revisit later.

Decode/audio internals are identical between `MC2Movie` and `MP4Player`
(cloned, not shared). The differences are texture ownership and
rendering responsibility.

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

## Project state and follow-ups

For current branch topology, collaboration status, upstream PRs, and
next concrete moves, read `devlogs/project_state.md`. Refreshed when
something material changes (merge, new PR, new external milestone) —
not on a calendar.

## Active devlogs (in progress)

Files in `devlogs/` root are active work. Move to `devlogs/closed/`
with a resolution note when done.

- [`briefing_map_black_textures_2026-04-25.md`](devlogs/briefing_map_black_textures_2026-04-25.md)
- [`mission_select_stray_lines_2026-04-25.md`](devlogs/mission_select_stray_lines_2026-04-25.md)

## Future follow-ups (not yet picked up)

Files in `devlogs/followups/`, one per topic. When work starts on a
follow-up, promote it to an active devlog in `devlogs/` root and
delete the follow-up file.

- [`alariq-pr-waits.md`](devlogs/followups/alariq-pr-waits.md) —
  per-PR status + outreach history for open upstream PRs.
- [`auto-detect-resolution.md`](devlogs/followups/auto-detect-resolution.md) —
  query `SDL_GetCurrentDisplayMode` on first launch to replace the
  hardcoded 1920×1080 in `options.cfg`.
- [`font-tweaks-residual.md`](devlogs/followups/font-tweaks-residual.md) —
  top-aligned screen headers sit too high; other in-game fonts may
  need eyeballing. Spawned when the D3F loader work closed.
- [`distribution-license-anchor.md`](devlogs/followups/distribution-license-anchor.md) —
  installer check is the full model (no runtime DRM); lean data-bundle
  pass (1.9 GB → 400-600 MB) before public release.
- [`mission-editor-tier0.md`](devlogs/followups/mission-editor-tier0.md) —
  revive 2001 mission editor on VS 2022 + MFC. Tier 0 = 2-5 days.
- [`release-installer.md`](devlogs/followups/release-installer.md) —
  Inno Setup installer; three polish items before public release
  (bundle ffmpeg, compression, x64compatible).
