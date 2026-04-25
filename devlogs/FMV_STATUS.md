# MC2 FMV Playback — Project Status

**Branch:** `fmv-mp4-support`
**Last updated:** 2026-04-23
**Audience:** Reviewer coming in cold to assess the video playback work.

> **2026-04-23 update — pilot-video crash fixed, all FMV surfaces verified.**
>
> **Root cause: `sws_scale` SIMD tail overrun.** `sws_scale` with `SWS_BILINEAR` overruns
> the RGBA destination buffer by up to ~32 bytes on widths that aren't SIMD-friendly (76-
> wide pilot-portrait clips are the classic trigger). `av_image_get_buffer_size(align=1)`
> returns exactly W*H*4, so the overrun corrupts heap metadata directly. Detected as
> `STATUS_HEAP_CORRUPTION (0xC0000374)` on the next heap op, typically inside
> `SDL_GL_SwapWindow`. Reproduces identically in the standalone `clean_mp4_player` on
> pilot clips (see commit `575afc6` for the full writeup).
>
> **Shipped this session:**
> - **Commit `575afc6`** — sws_scale tail-guard fix applied to `mp4player.cpp`,
>   `mc2movie.cpp`, and `clean_mp4_player.cpp`. Keeps `align=1` so linesize stays packed
>   for downstream memcpy/glTexSubImage2D, but allocates `rgbBuf`/`pendingData` with a
>   64-byte tail guard. MC2Movie also got pixel-store sanitation
>   (`glPixelStorei(GL_UNPACK_{ALIGNMENT,ROW_LENGTH})`) before its two `glTexSubImage2D`
>   sites, matching MP4Player's existing pattern.
> - **Commit `787cbb0`** — `ForceGroupIcon::bMovie` demoted from `static` to per-icon
>   member. A latent UAF bug separate from the heap corruption but found during
>   investigation. Invariant "at most one icon holds bMovie non-null at a time" owned
>   structurally by `ForceGroupBar::setPilotVideo`, with a defensive pre-allocation walk.
> - **Commit `1ed366f`** — removed dead `introMovie` (MC2Movie-based) field from
>   `mainmenu.{cpp,h}`. Migration to `introMP4Player` was incomplete; the old field
>   lingered with unreachable construction, teardown, and skip paths.
>
> **All FMV surfaces verified end-to-end in-game:**
> MSFT intro + CINEMA1-5 (`MP4Player` via `mainmenu.cpp` and `playFullScreenVideo`),
> pre-mission briefing (`MC2Movie` via mission selection), in-mission cinematics
> (`MC2Movie` via `controlgui.cpp`), pilot acknowledgement videos (`MP4Player` via
> `setPilotVideo`). The `prefs.pilotVideos = false` override has been removed.
>
> **Still open — not crash-related:**
> - `loop=true` in `forcegroupbar.cpp` pilot-video init. Pilot acks are one-shot
>   voice-overs; original intent unclear. Behavioral tweak, revisit separately.
> - `ControlGui::instance->isMoviePlaying()` arm in the `setPilotVideo` gate. Vanilla
>   2016 initial commit (= 2001 source) but whether it's correct or a pre-existing bug
>   that just never surfaced is unverified.
> - Step 8 of `FMV_DESIGN.md` §10 — delete `MP4Player` entirely (port pilot portrait,
>   credits, intro, full-screen cinematics over to `MC2Movie` first).

> **2026-04-22 update — briefing FMV polished, pilot-video path parked.**
>
> **Shipped this session (see commits `bc0d773`, `e071181`, `9ed1e5e`):**
> - Briefing FMV (MC2Movie path) is now **correctly positioned, docked to the top-right
>   corner of the screen at 1.5× scale, with bilinear filtering on the video quad**.
>   Three-layer fix in `controlgui.cpp`:
>   1. Un-baked `hiResOffsetX` from VideoStatic frame quads. The 1280/1600/1920
>      `buttonlayout.fit` files pre-applied the HiresOffsets X to border XLocations, so the
>      original engine only added `hiResOffsetX` to `videoRect` and not to `videoInfos`.
>      Our port was double-applying the offset. Fixed by snapshotting raw `.fit` HiresOffsets
>      into `fitBakedHiResOffsetX/Y` and subtracting them from the runtime offset passed to
>      `videoInfos[].init()`.
>   2. Bounding-box affine transform to dock + scale the entire panel (video + borders + text
>      rect) as a rigid group. Single `fmvScale = 1.5f` constant controls element sizes and
>      inter-element offsets together.
>   3. `gos_FilterBiLinear` on the video quad only (borders still point-filtered via
>      `StaticInfo::render`).
> - **OS cursor hidden** during gameplay (`SDL_ShowCursor(SDL_DISABLE)` after
>   `SDL_ShowWindow`). The game's in-engine arrow was double-drawing on top of the Windows
>   cursor before.
> - **Console auto-closes on game exit.** Removed the `getchar()` pause at end of `main()`.
>   Added unbuffered `freopen` redirect of stdout+stderr to `full_game/mc2_stdout.log` (`w`
>   mode, truncates each run). Diagnostics survive crashes; no need to keep the console
>   window alive manually.
>
> **Known issue — pilot portrait videos crash the game, currently disabled.**
> See **`memory/project_pilot_video_crash.md`** (handoff doc) and the `prefs.pilotVideos =
> false` force-override in `code/prefs.cpp`. Attack orders → pilot acknowledgements used to
> spawn an `MP4Player` via `forcegroupbar.cpp:setPilotVideo` → crash inside the first or
> few `MP4Player::update()` calls. With the override the game is stable; we'll tackle the
> UAF/FFmpeg-reentrancy root cause in a dedicated next session.
>
> **Still to do after pilot-video is fixed:** step 8 of `FMV_DESIGN.md` §10 — delete
> `MP4Player` entirely (port pilot portrait, credits, intro over to `MC2Movie` first).


> The older docs in this repo (`VIDEO_FIXES_SUMMARY.md`, `FMV_TESTING_GUIDE.md`) describe an
> abandoned earlier approach (MC2Movie + `video_position_fix` / `frame_rate_fix` helpers).
> They are out of date — do not trust them. This document reflects the current state.

> **2026-04-17 update — architecture pivot in progress.** Sections 3–7 below describe the
> direct-GL `MP4Player::render()` path. That path is being replaced with a new `MC2Movie`
> class (different design from the abandoned 2025 wrapper of the same name) that decodes
> into a gos-owned texture, with each call site emitting its own `gos_DrawQuads`. See
> **`FMV_DESIGN.md`** for the new architecture; that doc is authoritative for the migration.
> Migration progress (steps from `FMV_DESIGN.md` §10):
>
> - [x] Step 1 — `gos_GetTextureGLId` accessor in gameos
> - [x] Step 2 — `MC2Movie` class created (`code/mc2movie.{h,cpp}`); compiled into mc2.exe
> - [x] Step 3 — mission selection (`missionselectionscreen.{h,cpp}`) switched to `MC2Movie`
>   — **runtime-verified 2026-04-17** (intro + briefing-on-selection screen + voiceover all working)
> - [x] Step 4 — in-mission cinematics (`controlgui.cpp`) switched to `MC2Movie`
>   — build-verified, runtime-unverified (user looking for a mission with a known cutscene to test)
> - [ ] Steps 5–7 — pilot portrait, credits, intro (still on `MP4Player`)
> - [ ] Step 8 — delete `MP4Player`
>
> `MP4Player` still drives: pilot portrait, credits, intro.

> **Known UI-positioning issues surfaced during integration (not video-pipeline bugs):**
>
> - **[fixed in step 4 follow-up]** In-mission briefing video frame/video misalignment.
>   `controlgui.cpp` `.fit` loader was applying `hiResOffsetX` but not `hiResOffsetY` to
>   `videoRect` (lines around 2517-2522), and was calling `videoInfos[i].init()` with no
>   offsets at all (line 2539) — while every other `StaticInfo` array in the same loader
>   passed both offsets. Result: video shifted right of the surrounding border statics.
>   Fix is two lines: add Y offset to `videoRect`, pass both offsets to `videoInfos[i].init()`.
>   (`videoTextRect` at :2529-2530 has the same half-applied-offset bug but it's dead data
>   per `FMV_DESIGN.md` §8 — left untouched.)
>
> - **[deferred — not video-specific]** All `buttonlayout{640,800,1024,1280,1600,1920}.fit`
>   files are 4:3-authored. At a 16:9 resolution the game picks the layout whose width
>   matches and inherits its 4:3-tall coordinate space, so UI elements (including the
>   briefing video rect) end up positioned for a taller screen than is actually rendered.
>   This is a preexisting MC2 limitation that affects every UI element, not just video.
>   Fixing it = "widescreen UI support for MC2", a separate project: either author new
>   `.fit` files, implement aspect-aware scaling in the layout loader, or letterbox to 4:3.
>   Not blocking the FMV migration.

---

## 1. Goal

MechCommander 2 is an FMV-heavy 2001 game whose original video path is gone. We need MP4/H.264
playback for:

- Intro sequence: `MSFT.mp4` → `CINEMA1.mp4` → main menu
- Mission briefings (e.g. `NODE_02.mp4`) — rendered inside a briefing-box UI region
- Mission selection background (`STANDIN.mp4`)
- Credits
- Pilot portrait videos (e.g. `BUBBA1.mp4`, 76×54 px)

All videos should play at correct speed with synced audio, stop cleanly (no looping unless
intended), and not clobber the surrounding UI.

---

## 2. Current Status

### Working
- **Standalone player** (`build64/Release/clean_mp4_player.exe`) plays `MSFT.mp4` and
  `CINEMA1.mp4` correctly with audio and correct speed. This is the reference implementation.
- In-game intro sequence logic runs (MSFT → CINEMA1 → menu); sequencing works.
- Build produces `mc2.exe` and auto-copies it to `full_game/mc2.exe`.

### Last known issue (as of most recent build)
- In-game intro video showed **wrong colors / wrong position** when rendered in the game's GL
  context. Root causes diagnosed:
  1. Coordinate mismatch: game uses 800×600 *logical* coordinates
     (`Environment.screenWidth/Height` from `gameos.hpp`) but the GL window is physical
     (e.g. 1707×960). A rect of `(0,0,800,600)` was being drawn in physical space, ending
     up in the top-left corner.
  2. GL state contamination: the game's renderer leaves dirty state (`GL_UNPACK_ROW_LENGTH`,
     texenv mode, active texture unit), which corrupted video colors.
- **Fixes are in place** in `mp4player.cpp::render()` (logical→physical scaling, full
  save/restore of GL state, explicit `GL_REPLACE` texenv, pixel storage). The latest build
  was copied to `full_game/` but **has not been tested yet by the user**. This is the single
  most important thing for a reviewer to verify.

### Not yet validated
- Briefing video positioning inside the briefing UI box (`controlgui.cpp` path).
- Mission selection background video.
- Pilot portrait videos (tiny 76×54 — likely needs scissor/scaling care).
- Credits video.

---

## 3. Architecture (current)

Two-file design, modelled on ffplay / VLC's separate-packet-queue architecture.

```
MP4Player  (code/mp4player.{h,cpp})
├── FFmpeg demux + decode
├── std::queue<AVPacket*>   vidPktQueue      // raw video packets
├── std::queue<AudioChunk>  audPcmQueue      // decoded audio PCM
├── std::atomic<double>     audioClock       // master clock, written by SDL audio callback
├── SDL_AudioDeviceID       audioDevice      // starts paused, unpaused after pre-prime
├── GLuint                  textureID        // RGBA, direct glTexSubImage2D upload
└── Wall-clock fallback (SDL_GetPerformanceCounter) when hasAudio == false
```

`MC2Movie` (the old wrapper class) is **removed from the build**. `mp4player.cpp` does the GL
rendering directly.

### Decode / display loop (per update() call)
1. `SDL_GL_MakeCurrent()` — intro video runs from `MainMenu::render()` with game's context.
2. **Demux**: read up to ~80 packets. Video packets → `vidPktQueue`. Audio packets → decoded
   immediately to PCM and queued (audio needs to stay ahead of the callback).
3. **Start audio** once `audPcmQueue.size() >= 8` (pre-priming prevents initial crackle).
4. **If a pending frame exists and its PTS ≤ clock + 0.005s** → upload to GL texture, clear
   pending.
5. **Else** → decode one more video packet; if its PTS is due, upload directly; otherwise
   buffer as pending.
6. EOF / loop handling.
7. `render()`: textured quad in the displayRect, with full GL state save/restore.

### Clock
- With audio: `audioClock` is updated inside the SDL audio callback based on bytes consumed
  vs. sample rate and the latest PTS tagged onto each PCM chunk.
- No audio: wall-clock from `SDL_GetPerformanceCounter()` since start.
- Video frames are PTS-gated against whichever clock is active.

### Rendering specifics (`mp4player.cpp::render()`, around lines 430–510)
- Scales `displayRect` from 800×600 logical coords to the physical window using
  `Environment.screenWidth/Height` as the logical reference.
- Uses `glScissor` (not viewport manipulation) so the video can't paint outside its rect.
- Saves and restores: viewport, blend state, depth, scissor, texture2D enable, active
  texture unit, texenv mode, pixel storage (`GL_UNPACK_ALIGNMENT`, `GL_UNPACK_ROW_LENGTH`).
- Explicitly sets `GL_REPLACE` texenv so the video texture isn't modulated by whatever color
  the game last set.

### Critical integration points
- `mainmenu.cpp` — `MainMenu::begin()` constructs the `MP4Player`. `MainMenu::render()`
  has an **early return** that draws the video and skips the game's background render; without
  that the menu background paints over the video each frame.
- `controlgui.cpp` — briefing video path. Calls `bMovie->init(..., false, ...)` (no loop).
- `CMakeLists.txt` — two targets: `mc2` (main game) and `clean_mp4_player` (standalone test
  harness). Post-build step copies `mc2.exe` to `full_game/`.

---

## 4. History — What Was Tried (and why the current design exists)

The branch has **31 commits**. A compressed timeline of what didn't work and why:

| Approach | Why it failed |
|---|---|
| Interleaved demux/decode in one loop (original) | Audio and video competed for the same decode budget; whichever starved first hitched. |
| `SDL_Delay(1000/fps)` for frame pacing | Windows `SDL_Delay` granularity is ~15ms → regular 2–3× frame-time hitches. |
| Removing `SDL_Delay` entirely, no other timing | Ultra fast-forward (~430 fps) because decode rate = display rate. |
| `MC2Movie` wrapping `MP4Player` for rendering, timing split across both | State kept desyncing between the two classes. Color/position bugs multiplied. |
| `video_position_fix.cpp` + `frame_rate_fix.cpp` helpers | Band-aids on top of a broken pipeline. Removed. |
| Pre-buffering N video frames before starting | `CINEMA1` deadlocked: pre-buffered 1.2s of video but only 0.19s of audio; audio clock froze, frames never became "due". |
| Single packet queue with shared consumption | Same starvation problem — whichever stream lagged held up the other. |
| **Separate packet queues (current)** | Works. Audio and video are decoupled; each runs at its own rate. |

The standalone `clean_mp4_player.exe` was built specifically to validate the separate-queue
architecture in isolation before porting it into the game. That was the right call — several
game-only bugs (logical/physical coords, GL state contamination) were easy to isolate once
the decode pipeline was known good.

---

## 5. Key Files

| File | Role |
|---|---|
| `code/mp4player.h` / `.cpp` | **Core player** — FFmpeg decode, SDL audio, GL texture, timing, rendering. This is where 90% of the work lives. |
| `code/clean_mp4_player.cpp` | Standalone reference implementation. Proves the architecture. Build target: `clean_mp4_player`. |
| `code/mainmenu.h` / `.cpp` | Intro video sequencing. `MainMenu::render()` early-return is critical (see §3). |
| `code/controlgui.cpp` | In-game briefing video integration. Not yet validated end-to-end. |
| `CMakeLists.txt` | Build config. Defines both targets and the post-build copy to `full_game/`. |

All earlier dead code, scripts, and stale docs have been moved to
**`archive/fmv-dead-code/`** (see `archive/fmv-dead-code/README.md` for the index).
Nothing under `archive/` is referenced by the build. Summary of what's there:

- `archive/fmv-dead-code/code/` — `mc2movie.{cpp,h}`, `standalone_mp4_player.cpp`,
  `video_position_fix.{cpp,h}`, `frame_rate_fix.{cpp,h}`, and the unused FMV test framework
  (`fmv_test_framework.cpp`, `fmv_test_runner.cpp`, `mp4player_test_extension.{cpp,h}`,
  `test_metrics.h`, `video_validator.cpp`).
- `archive/fmv-dead-code/scripts/` — PowerShell drivers, `CMakeListsTestAddition.txt`,
  scratch logs.
- `archive/fmv-dead-code/docs/` — `VIDEO_FIXES_SUMMARY.md`, `FMV_TESTING_GUIDE.md`
  (superseded by this document).

`CMakeLists.txt` no longer references any of the above (the `mp4_standalone` and
`fmv_test_runner` targets were removed; `video_position_fix.cpp` / `frame_rate_fix.cpp`
were removed from the main `mc2` `SOURCES` list).

---

## 6. Build & Test

See `CLAUDE.md` for full prereqs. Short form:

```bash
cd build64
cmake --build . --config Release --target mc2
# auto-copies to full_game/mc2.exe

cmake --build . --config Release --target clean_mp4_player
# produces build64/Release/clean_mp4_player.exe
```

### Standalone test
```bash
build64/Release/clean_mp4_player.exe full_game/data/movies/MSFT.mp4
build64/Release/clean_mp4_player.exe full_game/data/movies/CINEMA1.mp4
```

### In-game test
Run `full_game/mc2.exe`. Intro should play MSFT → CINEMA1 → main menu with audio. ESC/Space/
click skips.

---

## 7. Where A Reviewer Should Look First

If you have limited time, in priority order:

1. **`code/mp4player.cpp` lines ~400–520** (`render()`). Verify the logical/physical scaling
   and GL save/restore correctness. This is the most recently changed and least-tested code.
2. **`code/mp4player.cpp::update()`**. Verify the decode loop handles both the audio-present
   and audio-absent cases correctly, and that `hasPending` / `pendingData` ownership is clean.
3. **`code/clean_mp4_player.cpp`** vs **`code/mp4player.cpp`** — diff the decode logic. If
   they've drifted, that's a hint something was changed during game integration that wasn't
   validated standalone.
4. **`code/mainmenu.cpp::render()`** — confirm the early-return-when-intro-playing path is
   correct and that the video player's GL context management doesn't conflict with the
   menu's own rendering when the intro finishes.
5. Known untested paths: briefing (`controlgui.cpp`), mission selection, credits, pilot
   portraits. Likely need similar logical/physical scaling attention and possibly
   different displayRect handling since they're not fullscreen.

### Open questions worth a second opinion
- Is the current audio-queue cap policy right? Earlier attempts used extreme buffering
  (16384 samples, 500 buffer queue). Current code uses 2048 samples, no artificial cap on
  the PCM queue. Standalone works; game integration may behave differently under load.
- Is it safe to rely on `Environment.screenWidth/Height` as the logical reference? It's
  populated by `gameos.hpp` at game startup — unsure whether it can be zero in early init
  (the code has a fallback to window size, but that fallback would produce the same bug we
  just fixed).
- Should video rendering restore **more** GL state than it currently does? The game's GL
  renderer is legacy fixed-function and fragile — there may be dirty state we haven't
  identified yet. Symptoms to watch for: UI text or sprite corruption immediately after
  video stops.

---

## 8. Glossary (MC2-specific)

- **FMV**: Full-Motion Video. Original MC2 used Bink/Smacker; we're replacing with MP4/H.264.
- **Environment**: global struct in `gameos.hpp` holding logical screen dimensions
  (800×600 in this codebase).
- **Logical vs physical coords**: Game UI is positioned in 800×600 logical space; the
  actual GL window can be any size. UI and video must scale accordingly.
- **MC2Movie**: defunct rendering wrapper class. Do not use. Present in old docs only.
