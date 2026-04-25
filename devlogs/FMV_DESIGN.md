# FMV Replacement Design

**Branch:** `fmv-mp4-support`
**Status:** Design only — no implementation yet.
**Authoritative spec:** `GameOS/include/gameos.hpp:482-488` (Microsoft's preserved `gosVideo_*` API, declarations intact, bodies stripped at open-sourcing).
**Supersedes:** the direct-GL rendering path currently in `code/mp4player.cpp`.

---

## 1. Summary

Video playback is restored by building a new `MC2Movie` class that decodes MP4/H.264 via FFmpeg into a **gos-owned texture**, which the game's existing UI renderer draws through its normal `gos_SetRenderState` + `gos_DrawQuads` pipeline. The shape is dictated by Microsoft's preserved `gosVideo_CreateResourceAsTexture(HGOSVIDEO*, DWORD* hTex, char* filename)` entry at `gameos.hpp:482-488`, which explicitly pairs a video handle with a texture handle the game then draws itself. The new class replaces `MP4Player`'s current direct-GL `render()` path (which manages its own texture, viewport, scissor, and GL state save/restore — see FMV_STATUS.md §3 "rendering specifics"). Headline architectural property: the player no longer knows about screen geometry. Each call site supplies vertex coordinates in its own logical space and the gos shader projection (`gameos_graphics.cpp:1613-1616`) converts uniformly.

---

## 2. The public API

Class name: **`MC2Movie`**. The original class of this name did not survive Microsoft's open-source drop; only its fossilized call-site shape remains at `code/forcegroupbar.cpp:458-459` (`//ForceGroupIcon::bMovie = new MC2Movie;`) and `code/backup.../mainmenu.cpp:173-174` (`introMovie = new MC2Movie; introMovie->init(path,movieRect,true);`). Alex's 2025 `mc2movie.{h,cpp}` was a distinct later rewrite and has been archived under `archive/fmv-dead-code/`; the name is free.

### 2.1 Required methods

Every verb below has at least one live caller in current MC2 code (sources from archaeology §Task 2).

| Method | Purpose | Evidence |
|---|---|---|
| `MC2Movie()` + `bool init(const char* path, RECT rect, bool loop)` | Open file, allocate gos texture at video dimensions, stash rect, start decode | Fossil `forcegroupbar.cpp:458-459`; live shape at `controlgui.cpp:2887`, `logistics.cpp:303,871`, `mainmenu.cpp:185`, `missionselectionscreen.cpp:263` |
| `void update()` | Pump decode; if a new frame's PTS is due, upload to the gos texture | `controlgui.cpp:946,2911`; `logistics.cpp:274`; `mainmenu.cpp:454`; `missionselectionscreen.cpp:141` |
| `bool isPlaying() const` | Returns `false` once the clip has reached EOF (replaces fossil `gos_Video::Update()→bool`'s done-signal) | `controlgui.cpp:947,2912`; `forcegroupbar.cpp:535`; `mainmenu.cpp:458` |
| `void stop()` | Immediate termination; releases decoder state | `logistics.cpp:271`; `mainmenu.cpp:418`; `mechicon.cpp:966`; `missionselectionscreen.cpp:136,141,256,269,273,336`; fossil `gos_Video::Stop()` |
| `void restart()` | Rewind to start | `mainmenu.cpp:192`; current `mp4player.h:34` |
| `const std::string& getMovieName() const` | Path accessor used for the hardcoded "cinema5 → credits" transition | `logistics.cpp:277,284` — string-compares against `"cinema5"` |
| `void setRect(RECT rect)` | Update stored destination rect without disturbing decoder state | `mechicon.cpp:973` (neutered call site; pilot portrait slots rearrange on mech death) |
| `DWORD getTextureHandle() const` | Return the gos texture handle callers bind with `gos_SetRenderState(gos_State_Texture, …)` | New; required by the quad pattern at FMV_RENDERING_RECON.md §1.1, matching the fossil `gosVideo_CreateResourceAsTexture` pairing |

### 2.2 Not in scope (fossil-only verbs, zero live callers)

Each verb below exists either in the current `MP4Player` header or in the GameOS fossils but has no caller in game code today. Explicitly omitted to keep the replacement minimal; origins recorded so future readers know they weren't overlooked.

- **`pause()` / `resume()`** — declared at `mp4player.h:34`. One live caller (`missionselectionscreen.cpp:377`, the `MN_MSG_PAUSE` button handler) was missed by the original survey. Resolution during step 3: drop the player-side action and keep the button toggle only — pause was never a meaningful UX for a ~30s briefing video, and adding it would require restartable audio-clock logic with no offsetting benefit. Fossil: `gos_Video::Pause()`, `gos_Video::Continue()`, `VideoManagerPause/Continue()`, `gosVideo_Pause`/`gosVideo_Continue` enum values.
- **`setVolume(float)`** — declared at `mp4player.h:37`, empty body at `mp4player.cpp:596`, no callers. Fossil: `gos_Video::m_volume`, `gosVideo_Volume` command.
- **`seek(double)`** — undeclared. Fossil: `gos_Video::FF(double)`, `VideoManagerFF(double)`, `gosVideo_SeekTime`. Note that `CPlayBIK::Execute` at `objective.cpp:1183-1187` also does not seek.
- **`setLocation(x,y)`, `setScale(sx,sy)`, `setPanning(p)`** — undeclared. Fossil: `gos_Video::SetLocation()`, `gosVideo_SetCoords`, `gosVideo_SetScale`, `gosVideo_Panning`.
- **`restore()` / `release()`** — DirectDraw surface-loss handling. Obsolete on SDL/GL.
- **`getFrameCount()`** — declared at `mp4player.h:41`, no callers.

---

## 3. GameOS integration: the one new API

The gos layer needs exactly one new function: a way to reach the raw `GLuint` inside a `DWORD` gos texture handle, so MC2Movie can target that texture with `glTexSubImage2D` directly. The existing `gos_LockTexture` / `gos_UnLockTexture` path (`gameos_graphics.cpp:730-787`) is unusable for video: its implementation does a GPU readback (`glGetTexImage`), a `new BYTE[w*h*4]` allocation, a full-frame RGBA↔BGRA software byte-swap, then the reverse swap and an upload — per frame. At 1920×1080 that's tens of millions of operations on the CPU path (FMV_RENDERING_RECON.md §5.1).

### 3.1 Declaration

In `GameOS/include/gameos.hpp`, alongside the other `gos_*Texture*` entry points:

```cpp
// Returns the underlying GL texture id for Handle, or 0 if invalid.
// Intended for fast-path per-frame uploads (e.g. video) using
// glBindTexture + glTexSubImage2D.
uint32_t __stdcall gos_GetTextureGLId( DWORD Handle );
```

Return type is `uint32_t` rather than `GLuint`: `gameos.hpp` includes no GL header, and the gos interior already uses this convention (`gosTexture::getTextureId()` at `gameos_graphics.cpp:727` returns `uint32_t`). The GL spec guarantees `GLuint` is a 32-bit unsigned int, so callers pass the return value directly to `glBindTexture`.

### 3.2 Implementation

In `GameOS/gameos/gameos_graphics.cpp` — **not** in the header. `class gosTexture` is file-local to that translation unit with no header declaration (FMV_RENDERING_RECON.md §5.2), so the implementation must live in the TU where `gosTexture` is visible:

```cpp
uint32_t __stdcall gos_GetTextureGLId( DWORD Handle ) {
    gosTexture* tex = g_gos_renderer->getTexture( Handle );
    return tex ? tex->getTextureId() : 0;
}
```

### 3.3 Safety

Alariq's gos layer is MC2-port-local code, not a closed-source Microsoft engine contract — the original `gos_Video` bodies were stripped at open-sourcing and everything currently in `GameOS/gameos/` is port-era. Adding a single read-only accessor is a non-breaking extension; no existing caller loses any guarantee.

---

## 4. Texture lifecycle

- **At `init()`:** MC2Movie calls `gos_NewEmptyTexture(gos_Texture_Alpha, name, RECT_TEX(width, height), 0)` to allocate an RGBA8 gos texture at the video's source dimensions. The returned `DWORD` handle is owned by this `MC2Movie` instance.
- **On each new frame:** FFmpeg decode → swscale to RGBA → `glBindTexture(GL_TEXTURE_2D, gos_GetTextureGLId(handle))` → `glTexSubImage2D(...)`. No readback, no byte-swap, no per-frame allocation — same cost as the current `mp4player.cpp:322,396` upload, just onto a gos-owned texture rather than a private one.
- **At destruction:** `gos_DestroyTexture(handle)`.

FMV_ARCHAEOLOGY.md §Task 3 confirmed end-to-end that `gos_NewEmptyTexture` → `gosTexture::createHardwareTexture()` (`gameos_graphics.cpp:865-924`) → `create2DTexture()` (`gl_utils.cpp:165-190`) → `glTexImage2D` accepts arbitrary `w, h > 0`. No POT snapping, no size rounding, no max below `GL_MAX_TEXTURE_SIZE`. Both 1920×1080 (intro, CINEMA*) and 76×54 (pilot portraits) work. The "512/256/128/64/32/16" restriction mentioned in the `gameos.hpp:2807` docstring is stale documentation.

---

## 5. Rendering: what call sites do

MC2Movie has **no** `render()` method. Callers obtain the texture handle and emit the quad themselves using the standard gos UI pipeline (FMV_RENDERING_RECON.md §1.1):

```cpp
gos_VERTEX v[4];
// Populate v[0..3] with caller's destination rect in caller's logical
// coord space. argb MUST be 0xffffffff — the gos_tex_vertex fragment
// shader multiplies vertex color into the sampled texel (shaders/
// gos_tex_vertex.frag:18: `c *= tex_color`), so any other argb tints
// the video. u/v = 0..1 across the quad.
gos_SetRenderState( gos_State_Texture,   movie->getTextureHandle() );
gos_SetRenderState( gos_State_Filter,    gos_FilterNone );
gos_SetRenderState( gos_State_AlphaMode, gos_Alpha_OneZero );
gos_DrawQuads( v, 4 );
// Note: gos render state is a global state machine, not a stack. The
// AlphaMode set above leaks to the next gos_DrawQuads issued by any
// caller. Either reset to gos_Alpha_AlphaInvAlpha here, or rely on the
// next UI element to set the mode it needs. First call site (§7.1)
// should include a comment noting this discipline.
```

This change buys back two properties `MP4Player`'s current `render()` gave up (both per FMV_RENDERING_RECON.md §3):

1. **No central coordinate-system assumption.** `mp4player.cpp:439-447` hardcodes a "logical 800×600" scaling path that is silently wrong wherever the caller's logical space differs (FMV_RENDERING_RECON.md §3.4, §5.5). Under the new design each caller supplies vertices in its own space — MainMenu's 1024×768 (`mainmenu.cpp:728-730`), ControlGui's per-resolution button-layout files (`controlgui.cpp:2649-2662`), LogisticsScreen's `Environment.screenWidth/Height` — and the gos shader projection converts each correctly.
2. **No GL state contamination.** The player stops touching viewport, scissor, blend, depth, active texture unit, texenv, or pixel storage; `gos_RendererBeginFrame` / `applyRenderStates` handle all of that uniformly. The fragile save/restore block currently in `mp4player.cpp::render()` (including `GL_UNPACK_ROW_LENGTH` and explicit `GL_REPLACE` texenv) is deleted.

The commented-out block preserved since Microsoft's initial commit at `missionselectionscreen.cpp:99-113` — confirmed by git blame to pre-date open-sourcing (archaeology §Task 4) — is literally this pattern. The refactor is a restoration of the original design, not a new invention.

---

## 6. Threading and audio (no changes)

Audio and decode threading are left exactly as in the current `MP4Player`. The SDL audio callback at `mp4player.cpp:11-37` touches only the PCM queue (mutex-protected) and the `std::atomic<double> audioClock` — no GL, no texture state, no rect, no handle; not a refactor target. FFmpeg demux + decode continue to run on the render thread, paced by the audio master clock with `SDL_GetPerformanceCounter()` wall-clock fallback when `hasAudio == false`. This matches RBDOOM-3-BFG's model (FMV_REFERENCE_NOTES.md §4.3) — single-threaded from the caller's view, audio feeding its own clock internally. OpenMW's three-thread parse/video/audio split is more robust but unnecessary for MC2's workload (one active video at a time, modest resolutions, no simultaneous multi-video). If decode starvation surfaces after integration, revisit then — not speculatively now.

---

## 7. Call-site changes

Implementation order: start with the site whose original design is explicitly preserved as comments (mission selection), then the two that need a real rect from layout data (briefing, pilot portrait), then fullscreen (credits), then the special-case intro last.

### 7.1 Mission selection (`code/missionselectionscreen.cpp`)

- **Location:** `missionselectionscreen.cpp:116` (currently `bMovie->render()`); `:89-113` (commented-out original draw block).
- **Coordinate system:** `rects[VIDEO_RECT]` populated during screen init (around `:245-248`); already in this screen's own space.
- **Currently:** `bMovie->render()` plus an early-return pattern upstream.
- **Becomes:** uncomment the preserved `gos_VERTEX v[4]` block at `:89-112`, replace the `videoTexture` binding target with `bMovie->getTextureHandle()`, leave the `operationScreen.render(...)` call at `:120` intact. No more early return — the video is just the topmost drawn element in the screen's normal z-order.

Why first: Microsoft preserved the exact target pattern in comments; the refactor is textually a comment-to-code restore.

### 7.2 Mission briefing (`code/controlgui.cpp`)

- **Location:** `controlgui.cpp:367-372` — the `if (moviePlaying && bMovie)` block, currently running `videoInfos[i].render()` then `bMovie->render()`.
- **Coordinate system:** `videoRect` from the `VideoWindow` block of the active `buttonlayout{640,800,1024,1280,1600,1920}.fit` file (`controlgui.cpp:2491-2496`), offset by `hiResOffsetX/Y`. Already in the active resolution's space.
- **Becomes:** after the `videoInfos[i].render()` loop (the briefing-box border statics), build a 4-vertex quad from `videoRect`, bind `bMovie->getTextureHandle()`, emit `gos_DrawQuads`. The video draws over the border; draw order is: panel backdrop → videoInfos border → video quad.

### 7.3 Pilot portrait (`code/mechicon.cpp`, `code/forcegroupbar.cpp`)

- **Location:** `mechicon.cpp:962-983` — the two-path block (`bMovie->render()` OR `pilotVideoTexture` static fallback).
- **Coordinate system:** `vRect` built per-slot from `bmpLocation[locationIndex]` + `pilotLocation[locationIndex]` at `:956-960`, in HUD space (shared with every other force-group icon). Underlying video is 76×54.
- **Currently:** `bMovie->render()` at `:974`; `setRect` call was commented out at `:973` in 2025 because the then-current wrapper had no such method.
- **Becomes:** the two branches collapse into one. Both the live-video branch and the static-portrait branch use `gos_SetRenderState(gos_State_Texture, handle) + gos_DrawQuads(v, 4)`; only the handle source differs (`bMovie->getTextureHandle()` vs `pilotVideoTexture`). Re-enable `bMovie->setRect(vRect)` at `:973` — in the new design `setRect` is a trivial stored-rect update, no decoder disturbance, so it's safe even when invoked per-frame as icons rearrange.
- **Constraint preserved:** `ForceGroupIcon::bMovie` is a single static pointer (`mechicon.cpp:33`); only one pilot-video decoder at a time across all 16 slots (FMV_RENDERING_RECON.md §5.8). Do not fan out to per-slot decoders.

### 7.4 Credits / campaign end (`code/logistics.cpp`)

- **Location:** `logistics.cpp:427-439` (currently early-return-when-video-playing; then `bMovie->render()`).
- **Coordinate system:** `{0, 0, Environment.screenWidth, 600}` from `:296-299`.
- **Becomes:** remove the early return; emit `gos_DrawQuads` with a 4-vertex quad covering that rect, after the rest of `Logistics::render()`.
- **Note, not fixed here:** the hardcoded `600` does not scale with resolution — a latent pre-existing bug. Explicitly **out of scope** for this design (see §8).

### 7.5 Intro (`code/mainmenu.cpp`)

Handled last, deliberately. Unlike the other four, MainMenu's early return at `:723-726` exists for a legitimate reason: the menu's own background must not draw while the intro is playing. The other early returns are workarounds; this one is design intent.

**Options (FMV_RENDERING_RECON.md §4.1, §5.6):**

- **(a)** Keep the early-return branch, but inside it, in place of `introMP4Player->render()`, build a fullscreen `gos_VERTEX[4]` from `{0, 0, Environment.screenWidth, Environment.screenHeight}` (`mainmenu.cpp:188`) and emit `gos_DrawQuads` against `introMovie->getTextureHandle()`. Minimal change; preserves menu-suppression intent.
- **(b)** Add a `menuSuppressed` flag checked inside `LogisticsScreen::render()`; let the intro video draw as a topmost z-ordered element through the normal UI pipeline.

**Recommendation: (a).** Smallest footprint, preserves existing intent.

---

## 8. What's not in scope

Each item deliberately omitted, with the reason:

- **`pause()`, `resume()`, `setVolume(float)`, `seek(double)`, `setLocation`, `setScale`, `setPanning`** — fossil-only verbs with zero live callers. See §2.2.
- **`restore()` / `release()`** — DirectDraw surface-loss handling. Obsolete on SDL/GL.
- **Hardcoded `600` height in `logistics.cpp:296-299`** — pre-existing bug, does not scale with resolution. Unrelated to video; file separately.
- **`videoTextRect` dangling field** — loaded at `controlgui.cpp:2498-2504` but never read anywhere in the codebase (archaeology §1B). Probably the original subtitle/caption rect; no surviving code says what should render into it, so no replacement here.
- **`movieCode` per-pilot variant selector** (`radio.cpp:348,172`; `controlgui.cpp:3216-3219`) — works today (single-char suffix concatenated onto pilot name, e.g. `"bubba" + "1" → "bubba1.mp4"`). Untouched.
- **Backend codec abstraction** — unlike RBDOOM-3-BFG (FMV_REFERENCE_NOTES.md §2.6), we do not introduce a `USE_FFMPEG` vs `USE_BINKDEC` compile-time split. FFmpeg only.
- **Fossil declarations in `GameOS/include/videoplayback.hpp` and `gameos.hpp:430-488`** (`gos_Video`, `VideoManager*`, `gosVideo_*`) — left in place. They have zero callers; harmless clutter. Removing them is a separate cleanup.

---

## 9. Open questions

Surfaced while drafting; not resolvable from the source documents alone.

1. **Texture format.** FMV_RENDERING_RECON.md §1.1 lists `gos_Texture_Alpha = 8888` (RGBA8). That matches what FFmpeg's swscale produces (`AV_PIX_FMT_RGBA`). Reasonable default; unverified whether original shipped videos were alpha-less RGB or palette, but that is a source-data question that does not affect the replacement.
2. **Should `init()` still take an `SDL_Window*` and `SDL_GLContext`?** Current `MP4Player::init(...)` accepts them because it manages its own GL state. Under the gos-texture design, upload targets whatever context `g_gos_renderer` is current-on; the player has no reason to know about window or context. **Resolved: drop both parameters from the signature.** The standalone player keeps its own MP4Player with those parameters if needed; the game-integrated class does not.
3. **MainMenu early-return disposition (§7.5).** Option (a) is recommended, but confirmation is needed that there is no scenario where menu elements should legitimately draw *over* the video.
4. **Per-frame audio queue policy under real game load.** FMV_STATUS.md §7 open question: is 2048 samples + uncapped PCM queue the right policy once the main-thread budget is contested? Only observable post-integration.

*(Earlier Q4 — whether the gos shader multiplies vertex argb into the sampled texel — was resolved during implementation of step 1. Answer: yes. `shaders/gos_tex_vertex.frag:18` does `c *= tex_color`, so `argb = 0xffffffff` is mandatory. Reflected in the §5 snippet.)*

---

## 10. Implementation order

Shortest path to end-to-end validation first; riskiest site last.

1. **[done]** Add `gos_GetTextureGLId(DWORD)` declaration in `GameOS/include/gameos.hpp` and implementation in `GameOS/gameos/gameos_graphics.cpp` (§3).
2. **[done]** Create `MC2Movie` in `code/mc2movie.{h,cpp}`. Reuse the existing FFmpeg demux + decode + SDL audio code from `MP4Player` (that part works — §6); what's new is texture ownership via `gos_NewEmptyTexture` and upload via `glTexSubImage2D` against `gos_GetTextureGLId(handle)`. Keep `MP4Player` compiled and working in parallel; do not delete it yet.
3. **[done, runtime-verified 2026-04-17]** Integrate at **mission selection** (§7.1). Validates the full pipeline with the least risk — original design preserved as comments; caller already knows how to build `gos_VERTEX[4]`.
4. **[done, build-verified, runtime-unverified]** Integrate **mission briefing** (§7.2 — in-mission cinematics in `controlgui.cpp`). Validates non-fullscreen rect + surrounding UI elements.
5. Integrate **pilot portrait** (§7.3). Validates `setRect` and tiny textures (76×54). Closes the `mechicon.cpp:973` gap.
6. Integrate **credits** (§7.4). Validates fullscreen with a different owner (`Logistics` rather than `MainMenu`).
7. Integrate **intro** (§7.5). Handles the menu-suppression question last, after the simpler sites have proven the pipeline.
8. Delete the `MP4Player` direct-GL render path; `MC2Movie` becomes canonical. Decode and audio internals migrate intact.
9. Update `CLAUDE.md` — the "MP4 Video Playback" section currently points at `MP4Player` and `archive/fmv-dead-code/` for the previous dead `MC2Movie`; update it to name the new `MC2Movie` class and move `MP4Player` into the same archive pointer.

---

*End of design.*
