# MC2 UI / Sprite Rendering — Reconnaissance Report

**Purpose:** Decide whether `MP4Player` can be refactored to produce a texture that the game's existing UI renderer draws, instead of drawing directly to GL.

**Scope:** Pure investigation. No files were modified.

---

## Executive Summary

- The game has exactly **one** UI textured-quad pipeline: `gos_SetRenderState(gos_State_Texture, handle)` → `gos_DrawQuads(location, 4)`. Every menu, button, briefing panel, pilot portrait, mission-selection background goes through it.
- Under the hood, a `DWORD` texture handle is an index into `gosRenderer::textureList_`; the renderer binds `gosTexture::tex_.id` (the raw GL texture ID) via `glBindTexture` in `applyRenderStates()` at `gameos_graphics.cpp:1566`. So every "gos texture" **is already a real GL texture**.
- There is a pre-existing commented-out implementation of the mission-selection video drawing as exactly this pattern: `gos_SetRenderState(gos_State_Texture, videoTexture); gos_DrawQuads(v, 4);` (`missionselectionscreen.cpp:99-112`). The `pilotVideoTexture` fallback path (`mechicon.cpp:977-983`) actively uses this pattern right now. The refactor is a return to the original design.
- **Verdict: moderately painful but clean.** Two real pains: (1) the per-frame texture upload path via `gos_LockTexture` is not viable (double byte-swap + readback + full-frame `new[]`/`delete[]` every frame); you'll want one small new API — a function to get the raw `GLuint` from a `DWORD` gos handle — so `glTexSubImage2D` can target it directly. (2) Every video call site currently bypasses game rendering via an early-return (MainMenu, Logistics, ControlGui) and relies on MP4Player scaling to different logical-vs-physical conventions; unwinding those early-returns and matching each site's coordinate system is the bulk of the work.
- **Biggest risk:** coordinate systems are not uniform across the game. MainMenu uses a 1024×768 logical reference (`mainmenu.cpp:728`), ControlGui loads per-resolution button-layout files (`buttonlayout{640,800,1024,1280,1600,1920}.fit`), and vertex coords for `gos_DrawQuads` are in whatever `Environment.screenWidth/Height` currently says. FMV_STATUS.md's "800×600 logical" claim is incomplete — more on this in §3.

---

## Part 1: How the game draws a 2D textured quad

### 1.1 The single pipeline

Every 2D textured draw in the game follows this pattern:

```cpp
// gui/asystem.cpp:416 — aObject::render()
unsigned long gosID = mcTextureManager->get_gosTextureHandle( textureHandle );
gos_SetRenderState( gos_State_Texture, gosID );
gos_SetRenderState( gos_State_Filter, gos_FilterNone );
gos_SetRenderState( gos_State_AlphaMode, gos_Alpha_AlphaInvAlpha );
gos_SetRenderState( gos_State_ZCompare, 0 );
gos_SetRenderState( gos_State_ZWrite, 0 );
gos_DrawQuads( location, 4 );
```

- **Input:** `textureHandle` (an MC texture-manager node index, `unsigned long`, range 0..4095) and `location[4]` (four `gos_VERTEX`s defining the quad in screen coords).
- **Coordinates:** `gos_VERTEX::x/y` "must be 0.0 to `Environment.screenWidth/Height`" (verbatim from `gameos.hpp:2125`). Shader projection handles the transform to NDC.
- **Setup/teardown:** none local — render state is global and persists across draw calls. Callers just set whichever states they care about.
- **Texture format constraints:** width must be a power of two and no larger than 256 in the historical pipeline (see `txmmgr.h:5-8` comment and `MAX_MC2_GOS_TEXTURES`); modern calls via `gos_NewEmptyTexture` accept arbitrary W×H via `RECT_TEX(w,h)` macro at `gameos.hpp:2799`.
- **Arbitrary RGBA textures:** yes, with the right `gos_TextureFormat` (`gos_Texture_Alpha` = 8888). Palette formats are supported but optional.
- **Where can it be called from:** inside the `UpdateRenderers()` callback that `gameosmain.cpp:154` invokes between `gos_RendererBeginFrame()`/`gos_RendererEndFrame()`. UI code all runs inside that window.

### 1.2 Buttons

`gui/abutton.cpp:148`:
```cpp
void aButton::render() {
    if ( state != HIDDEN ) {
        if ( textureHandle ) {
            unsigned long gosID = mcTextureManager->get_gosTextureHandle( textureHandle );
            gos_SetRenderState( gos_State_Texture, gosID );
        }
        // ... extra alpha/filter state ...
        gos_DrawQuads( location, 4 );
```

Identical pattern. Buttons are just `aObject`s with their own handlers.

### 1.3 Main menu background

`MainMenu` is a `LogisticsScreen`. `LogisticsScreen::render()` at `gui/logisticsscreen.cpp:334` iterates over member collections:
```cpp
for (int i = 0; i < rectCount; i++) rects[i].render();   // rectangles
for (int i = 0; i < staticCount; i++) statics[i].render();  // statics (bitmap backgrounds)
for (int i = 0; i < buttonCount; i++) buttons[i].render();
for (int i = 0; i < textCount; i++) textObjects[i].render();
// etc — all aObject-derived
```
Every one of those `.render()` calls ends up in the `aObject::render()` → `gos_DrawQuads` path above. The menu background is one or more `statics[i]` entries whose `textureHandle` was loaded from a FitIniFile via `aObject::init(FitIniFile*, const char*, DWORD)` (`asystem.h:115`).

### 1.4 Briefing panel (ControlGui)

The briefing UI is drawn by `ControlGui`. When no video is playing, the panel background and buttons go through `aObject::render()` as above. When a video IS playing (`controlgui.cpp:367`):
```cpp
if ( moviePlaying && bMovie ) {
    for ( int i = 0; i < videoInfoCount; i++ )
        videoInfos[i].render();        // surrounding static graphics (briefing box border etc)
    bMovie->render();                  // MP4Player direct-GL render, bypasses gos entirely
}
```
`videoRect` is loaded from a `VideoWindow` block of the current button-layout FitIniFile (`controlgui.cpp:2491-2496`), pre-offset by `hiResOffsetX/Y`. The rect is in that layout file's coordinate system (which tracks the active resolution).

### 1.5 Pilot portraits — two paths already coexist

`code/mechicon.cpp:962-983`:
```cpp
if ( bMovie ) {
    if ( unit->getStatus() == OBJECT_STATUS_SHUTDOWN ) {
        bMovie->stop(); delete bMovie; bMovie = NULL;
    } else {
        bMovie->render();  // MP4Player direct-GL
    }
}
else if ( pilotVideoTexture ) {
    gos_SetRenderState( gos_State_Texture, pilotVideoTexture );
    v[1].v = v[2].v = .4140625;
    gos_DrawQuads( v, 4 );  // normal game pipeline — this is the pattern we want for video
}
```

`pilotVideoTexture` is a raw `DWORD` gos texture handle declared at `mechicon.cpp:34`. When there's no video, the static portrait renders through `gos_DrawQuads`. When there's a video, MP4Player takes over and does its own GL draw. This dual path is explicit evidence that the game's existing UI code is perfectly happy drawing a per-unit texture as a quad — the video version just currently opts out of it.

---

## Part 2: Texture lifecycle

### 2.1 Two tiers of texture management

**Tier A — `MC_TextureManager` (`mclib/txmmgr.h/.cpp`):** high-level asset cache. Keyed on file path; supports LZ-compressed idle storage with on-demand upload; a cache of up to `MAX_MC2_GOS_TEXTURES = 750` live gos handles is maintained (`txmmgr.h:46`). `loadTexture(path, format, hints)` returns a node index; `get_gosTextureHandle(nodeID)` resolves that to a live gos handle, decompressing and uploading if necessary. This is what `aObject::textureHandle` refers to. **Not suitable for per-frame video updates** — `textureFromMemory()` at `txmmgr.cpp:1703-1766` LZ-compresses the input and only supports square textures (`width*width*bitDepth`).

**Tier B — `gosRenderer` (`GameOS/gameos/gameos_graphics.cpp`):** low-level texture registry. `DWORD` handles are indices into `textureList_`. Each entry is a `gosTexture*` that owns a single GL texture. Texture creation funnels through `gos_NewEmptyTexture` / `gos_NewTextureFromMemory` / `gos_NewTextureFromFile`. `applyRenderStates` resolves the handle:
```cpp
// gameos_graphics.cpp:1560-1567
DWORD gosTextureHandle = renderStates_[tex_states[i]];
glActiveTexture(GL_TEXTURE0 + i);
gosTexture* tex = (...INVALID...) ? 0 : this->getTexture(gosTextureHandle);
if (tex) {
    glBindTexture(GL_TEXTURE_2D, tex->getTextureId());
```

### 2.2 Dynamic updates — what's available, what works, what doesn't

- **`gos_LockTexture` / `gos_UnLockTexture` (`gameos_graphics.cpp:2379, 2402`)** — the public dynamic-update API. But look at `gosTexture::Lock()` at `gameos_graphics.cpp:730-763`:

  ```cpp
  // gameos_graphics.cpp:748-762
  const uint32_t ts = tex_.w*tex_.h * getTexFormatPixelSize(TF_RGBA8);
  plocked_area_ = new BYTE[ts];
  getTextureData(tex_, 0, plocked_area_, TF_RGBA8);   // glGetTexImage — GPU readback
  for(int y=0; y<tex_.h; ++y) {
      for(int x=0; x<tex_.w; ++x) {
          DWORD rgba = ((DWORD*)plocked_area_)[...];
          // swap RGBA -> BGRA in software
          ((DWORD*)plocked_area_)[...] = bgra;
      }
  }
  return plocked_area_;
  ```
  `Unlock()` at line 765 does the reverse byte-swap and calls `updateTexture()` to re-upload. For 1920×1080 that's a `glGetTexImage` readback + `new BYTE[8.3MB]` + 2M-iteration byte-swap loop + another 2M-iteration byte-swap + `glTexSubImage2D` — per frame. **Unusable for video.**

- **`updateTexture(const Texture& t, void* pdata, TexFormat)` (`GameOS/gameos/utils/gl_utils.cpp:258`)** — the underlying machinery is exactly `glBindTexture + glTexSubImage2D`. No readback, no byte-swap:
  ```cpp
  glBindTexture(GL_TEXTURE_2D, t.id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t.w, t.h, fmt, ch_type, pdata);
  ```
  But `Texture` (the internal one, not `gosTexture`) is not exposed via the public gos API.

- **Direct `glTexSubImage2D`** — MP4Player already does this on its own texture today (`mp4player.cpp:322, 396`). That's what you'd want to keep doing — just targeting the GL ID owned by a gos texture instead of a private one.

### 2.3 Existing precedent for dynamically-updated textures

Minor. I found no examples of the game updating a texture every frame through the public gos API — everything is load-once. `gos_StartRenderToTexture` / `gos_EndRenderToTexture` (`gameos.hpp:2879-2885`) are documented for shadow-map rendering but limited to writing a single alpha bit. The `pilotVideoTexture` DWORD field exists but is currently used only for static portraits in the no-video fallback. **The video case would be the first per-frame-updated UI texture in the codebase.**

---

## Part 3: Coordinate system reality

### 3.1 FMV_STATUS.md's claim is partly right, partly misleading

FMV_STATUS.md says: "game uses 800×600 logical coordinates, scaled to physical window size". That's **approximately true for the main menu only**, and FMV_STATUS.md's assumption drives `MP4Player::render()`'s scaling code at `mp4player.cpp:439-447`. The reality is more varied.

### 3.2 Where scaling actually happens

**Not in each draw call.** `gos_DrawQuads` receives vertices in `Environment.screenWidth/Height` coordinate space (comment at `gameos.hpp:2125`). The gos renderer's shader projection handles the transform to NDC:
```cpp
// gameos_graphics.cpp:1613-1616 — gosRenderer::handleEvents rebuilds projection on resize
projection_ = mat4(2.0f / (float)width_, 0, 0.0f, -1.0f,
                   0, -2.0f / (float)height_, 0.0f, 1.0f,
                   0, 0, 1.0f, 0.0f,
                   0, 0, 0.0f, 1.0f);
```
The GL viewport itself is set to the *physical* drawable size at the start of each frame:
```cpp
// gameosmain.cpp:111-113
const int viewport_w = Environment.drawableWidth;
const int viewport_h = Environment.drawableHeight;
glViewport(0, 0, viewport_w, viewport_h);
```
So the game cleanly separates logical (UI coords) and physical (GL viewport) with the shader projection bridging them. MP4Player's manual logical→physical scaling (`mp4player.cpp:441-442`) is re-inventing something the game already does — if video rendered through `gos_DrawQuads` it would pick up the correct transform for free.

### 3.3 Environment population

- Initial values set at startup: `mechcmd2.cpp:2708-2709` sets `Environment.screenWidth/Height = 800/600`. `logmain.cpp:780` sets 640/480 earlier.
- Window resize updates them: `gameos_graphics.cpp:1622-1625`:
  ```cpp
  Environment.screenWidth = width_;
  Environment.screenHeight = height_;
  graphics::get_drawable_size(win_h_, &Environment.drawableWidth, &Environment.drawableHeight);
  ```
- **Can it be zero during early init (intro video time)?** Unlikely in normal flow — `mechcmd2.cpp:2708` runs long before the intro. But defensive fallback is in place in MP4Player at `mp4player.cpp:439-440`:
  ```cpp
  int logicalW = Environment.screenWidth > 0 ? Environment.screenWidth : windowW;
  int logicalH = Environment.screenHeight > 0 ? Environment.screenHeight : windowH;
  ```
  — which would collapse the scaling to identity, producing the very bug the fallback was supposed to prevent. Question flagged in §5.

### 3.4 Per-screen logical references are NOT consistent

Different UI screens use different "logical" baselines:
- **MainMenu** — `mainmenu.cpp:728-730`:
  ```cpp
  float scaleX = Environment.screenWidth / 1024.0f;
  float scaleY = Environment.screenHeight / 768.0f;
  ```
  So MainMenu assumes 1024×768 is its authoring resolution.
- **ControlGui** — loads button coords from `buttonlayout{640,800,1024,1280,1600,1920}.fit` at `controlgui.cpp:2649-2662`, plus `hiResOffsetX/Y`. Coords are already in the current resolution.
- **LogisticsScreen** (generic) — vertices stored in `Environment.screenWidth/Height` space directly; fade-rect code uses `{0, 0, Environment.screenWidth, Environment.screenHeight}` (`logisticsscreen.cpp:383`).

No single answer. A refactored video path that emits a gos quad wouldn't care, because vertex coords would be supplied by the caller in whatever space the caller uses. The current MP4Player, trying to scale centrally, hits this mismatch.

---

## Part 4: Video call sites

### 4.1 Intro (MainMenu)

- **Owner:** `MainMenu::render()` at `mainmenu.cpp:720`.
- **Rect:** fullscreen: `RECT movieRect = {0, 0, Environment.screenWidth, Environment.screenHeight}` (`mainmenu.cpp:188`).
- **Surrounding UI:** none — intro intentionally suppresses the menu. The early return at `mainmenu.cpp:723-726` skips all normal menu rendering while intro is playing.
- **Fit for gos-quad path?** Yes, trivially. Fullscreen quad at `(0,0)-(screenW,screenH)`. Could just replace `introMP4Player->render()` with `gos_SetRenderState(gos_State_Texture, videoHandle); gos_DrawQuads(verts, 4);`.

### 4.2 Mission briefing (ControlGui)

- **Owner:** `ControlGui::render()` at `controlgui.cpp:367-372`. Surrounding `videoInfos[i].render()` calls for border/frame.
- **Rect:** `videoRect` from `VideoWindow` block of current button-layout file, offset by `hiResOffsetX`.
- **Surrounding UI:** ControlGui draws tabs, buttons, objectives ALL in the same render pass; the video is painted on top of its designated area.
- **Fit for gos-quad path?** **Ideal fit.** The commented-out code at `missionselectionscreen.cpp:99-112` shows a sibling screen already had exactly this pattern. Would slot right in.

### 4.3 Mission selection background

- **Owner:** `MissionSelectionScreen::render()` at `missionselectionscreen.cpp:115-116`.
- **Rect:** `rects[VIDEO_RECT]` (`missionselectionscreen.cpp:245-248`).
- **Surrounding UI:** the operation screen, mission text, buttons — `operationScreen.render(xOffset, yOffset)` on line 120 runs AFTER the movie.
- **Fit for gos-quad path?** **Ideal.** The commented-out code at `missionselectionscreen.cpp:99-112` is literally this refactor, preserved in a comment:
  ```cpp
  gos_SetRenderState( gos_State_Texture, videoTexture );
  gos_DrawQuads( v, 4 );
  ```
  The caller already knows how to populate a `gos_VERTEX v[4]` correctly.

### 4.4 Credits

- **Owner:** `Logistics::render()` at `logistics.cpp:427-439`. Again an early-return-when-video-playing pattern.
- **Rect:** fullscreen-ish: `{0, 0, Environment.screenWidth, 600}` (`logistics.cpp:296-299`). Height is hardcoded 600 regardless of actual resolution — latent bug unrelated to the refactor, but worth flagging.
- **Fit for gos-quad path?** Yes — fullscreen-ish quad.

### 4.5 Pilot portraits

- **Owner:** `ForceGroupIcon::render` (around `mechicon.cpp:940-985`), per-unit in the HUD. Called once per force-group slot.
- **Rect:** `vRect` built from `bmpLocation[locationIndex]` and `pilotLocation[locationIndex]` (`mechicon.cpp:956-960`). Tiny — underlying video is 76×54.
- **Surrounding UI:** every other force-group icon, selection box, health bar, status ring — all drawn in the same loop as part of the HUD pass.
- **Fit for gos-quad path?** **Already partially designed for it.** Line 977-983 already does exactly the gos-quad pattern for the static fallback. Replacing `bMovie->render()` on line 974 with the same pattern (pointing at a video-backed gos texture) fits the existing structure.

### 4.6 Threading

Single-threaded render pass. `gameosmain.cpp:158-159` calls `gos_RendererBeginFrame`, then `Environment.UpdateRenderers()` (defined in `mechcmd2.cpp:681`), which invokes all the screen `.render()` methods. `MP4Player::update()` and `render()` happen on that same thread. The **only** threaded component is the SDL audio callback, which only touches the audio PCM queue + the atomic `audioClock` (`mp4player.cpp:11-37`) — no GL, no texture state. Not a blocker for the refactor.

---

## Part 5: Blockers and weird stuff

### 5.1 The `gos_LockTexture` byte-swap catastrophe

Detailed in §2.2. The only current public API for per-frame texture updates is `gos_LockTexture` / `gos_UnLockTexture`, and its implementation at `gameos_graphics.cpp:730-787` does a GPU readback plus two full-frame software byte-swap passes per frame. A refactor relying on this will perform worse than the current direct-GL approach. **Need a way to get the raw `GLuint` out of a `DWORD` gos handle** (or a fast-path texture-update API added to gos).

### 5.2 The `gosTexture` class is file-local

`class gosTexture` at `gameos_graphics.cpp:650` has no header declaration — it's defined inline in the .cpp. You can't `#include` it from `mp4player.cpp`. `gos_GetTextureInfo` exists (`gameos.hpp:2799 ff.`) but only exposes width/height/format, not the GL ID. Adding a small API like:
```cpp
GLuint __stdcall gos_GetTextureGLId( DWORD Handle );
```
in `gameos.hpp` (one-line wrapper calling `g_gos_renderer->getTexture(Handle)->getTextureId()`) would unblock the refactor cleanly. Not a breaking change.

### 5.3 MC_TextureManager doesn't want dynamic textures

`MC_TextureManager` (tier A above) cannot host a live video texture — `textureFromMemory()` compresses and caches, `get_gosTextureHandle()` re-decompresses on demand. But we don't need to use it. The game already has call sites (the `pilotVideoTexture` DWORD at `mechicon.cpp:34`) that hold a raw tier-B gos handle and pass it directly to `gos_SetRenderState`. That's the pattern video should follow.

### 5.4 Power-of-two / square texture concerns

Some legacy comments (`txmmgr.h:5-8`, `txmmgr.h:43`) and the `gos_NewEmptyTexture` docstring (`gameos.hpp:2805-2820`) mention "square textures" and "sizes 512/256/128/64/32/16". In practice the `RECT_TEX(width,height)` macro (`gameos.hpp:2799`) exists for non-square textures and the current renderer accepts arbitrary dimensions — `gos_NewEmptyTexture` at `gameos_graphics.cpp:2334` extracts `w = HeightWidth & 0xffff; h = HeightWidth >> 16` when the high 16 bits are set, and the underlying `createHardwareTexture` just calls `glTexImage2D` with whatever dimensions. So 1920×1080 video textures or 76×54 pilot-portrait video textures should Just Work. I did not find non-power-of-two restrictions in the GL backend. Small risk that one of the texture format codepaths silently snaps sizes — worth testing with a 1920×1080 empty texture before committing to the design.

### 5.5 Coordinate-system confusion in MP4Player is currently a tangible bug

MP4Player's `render()` at `mp4player.cpp:431-537` does manual logical→physical scaling based on `Environment.screenWidth/Height`. This is:
- correct when `Environment.screenWidth` genuinely is the game's logical reference (most scenarios);
- **silently broken** when `Environment.screenWidth == windowW` (e.g., the game runs at native resolution), because then `scaleX = 1.0` and the vertex coordinates are consumed as physical pixels — which happens to match callers who passed physical-pixel rects, but not callers who passed 800×600 rects. Different call sites in the current code pass rects in different spaces (§3.4), so the current single `MP4Player::render()` is fundamentally in the wrong place to do scaling.
- The GL viewport is also re-set in `render()` (`mp4player.cpp:469`), then restored at the end. If anything in the game expected the frame-start viewport to persist, this would corrupt it — not currently observed but fragile.

Emitting a gos texture and letting the caller place the quad sidesteps all of this.

### 5.6 The "early return steals the frame" pattern

Three call sites use this pattern (`mainmenu.cpp:723-726`, `logistics.cpp:429-432`, `controlgui.cpp:367-372` — the ControlGui case is the least bad because it renders videoInfos first). The early return was added deliberately to stop the menu/logistics background from painting over the video. If video rendered through `gos_DrawQuads` in z-order with other UI, the early return would be removed entirely — the video would simply be the topmost drawn element in the existing render loop. Small win for correctness, removes a fragile hack.

### 5.7 Audio threading

Already handled. `MP4Player::audioCallback` (`mp4player.cpp:11-37`) only touches the audio PCM queue (protected by `audioMutex`) and the `std::atomic<double> audioClock`. No GL calls. The refactor doesn't touch the audio side.

### 5.8 Multiple simultaneous videos

`ForceGroupIcon::bMovie` is a *single static pointer* (`mechicon.cpp:33`), not per-unit. So only one pilot portrait at a time can have a video; the other 15 force-group slots use the static `pilotVideoTexture` fallback. Any refactor should preserve this constraint — you don't want to allocate 16 FFmpeg decoders.

---

## Recommendation

**Moderately painful but clean.** The game's UI rendering is designed around the pattern the refactor wants. Commented-out code in `missionselectionscreen.cpp:99-112` and the active fallback in `mechicon.cpp:977-983` show the original design contemplated this. The existing direct-GL approach in MP4Player is a workaround introduced while getting video working at all; replacing it with a gos-texture-backed design puts it back on the intended rails.

### The pain

1. **One new gos API function.** Need a way to reach the raw `GLuint` from a `DWORD` gos handle without going through the lock/unlock readback path. Smallest possible change: add `gos_GetTextureGLId(DWORD handle)` alongside the existing texture APIs. Three lines of code plus a header entry.

2. **MP4Player restructure.** `update()` stops uploading to its own texture and instead uploads to the gos-owned texture (given its GL ID). `render()` goes away entirely — it's the caller's job now.

3. **Every call site touched.** MainMenu, Logistics, ControlGui, MissionSelectionScreen, ForceGroupIcon. Remove the early-return-when-video-playing hacks. Replace `bMovie->render()` with `gos_SetRenderState + gos_DrawQuads` using the call-site's native coordinate system. ControlGui and MissionSelectionScreen and ForceGroupIcon already know how to build `gos_VERTEX[4]` arrays. MainMenu and Logistics currently don't and would need a small helper.

4. **Lifecycle management.** MP4Player currently owns its GL texture and deletes it on destruction (`mp4player.cpp:220`). Under the refactor it would own a gos handle instead (`gos_NewEmptyTexture` + `gos_DestroyTexture`), which is a cleaner ownership story but is a behavior change worth being deliberate about.

### What the refactor does NOT break

- Audio (lives in a separate thread, touches nothing visual).
- Decode timing (`decodeVideo()`, the PTS/audio-clock machinery, the packet queues are orthogonal).
- `clean_mp4_player.exe` keeps its direct-GL render path — it doesn't depend on gos and is the reference.

### Recommendation summary

Yes, do it, but plan for a small gos API addition. Without it, the refactor is technically possible using `gos_LockTexture`/`gos_UnLockTexture`, but the resulting per-frame cost makes it strictly worse than the current implementation.

---

## Open questions I couldn't answer from reading the code

1. **Does the gos renderer cope with very large non-power-of-two textures?** The 1920×1080 intro case would need a 1920×1080 gos texture. Allocation path goes through `gos_NewEmptyTexture` → `new gosTexture(..., w=1920, h=1080, ...)` → `createHardwareTexture()`. I did not trace `createHardwareTexture()` to confirm it uses `glTexImage2D` with arbitrary dimensions (vs. snapping to POT or enforcing a max). Needs a quick test.

2. **Does the gos shader for `gos_tex_vertex` apply alpha blending in a way that would tint video frames?** The material name is looked up at `gameos_graphics.cpp:1648-1665`. If the shader multiplies the texture by `argb` from the vertex, callers would need to set `gos_VERTEX::argb = 0xffffffff` to avoid tinting. Easy to handle but good to confirm before reviewing call-site patches.

3. **What's the authoritative "logical" coordinate reference for the intro?** Main menu uses 1024×768 (`mainmenu.cpp:728`); the intro currently uses `{0, 0, Environment.screenWidth, Environment.screenHeight}` (`mainmenu.cpp:188`). Should the refactored intro quad continue to use `screenWidth/Height`, or match the 1024×768 baseline the menu uses? Probably the former (intro is pre-menu), but worth confirming with someone who knows the design history.

4. **Why was the `pilotVideoTexture` static-fallback kept instead of wiring video through it?** The commented-out `//bMovie->setRect(vRect);` on line 973 suggests someone tried to unify the paths and backed out. No context on why. Might uncover a constraint we're missing.

5. **Power-of-two constraint history.** The `txmmgr.h:5-8` comment says "NOT ANY MORE" about coalescing smaller textures, implying the POT restriction was loosened. But `gos_NewEmptyTexture`'s docstring (`gameos.hpp:2807`) still says "Only textures with sizes already allocated in heaps can be created (ie: 512, 256, 128, 64, 32 or 16*16)". Which is currently authoritative? A grep for actual runtime POT enforcement code would settle it.
