# MC2 FMV Archaeology Report

**Purpose:** Map the fossilized imprint of the original video system so a replacement can honor the original contract.

**Key finding up front:** The original video system was **DirectShow-based** (not Bink, despite the naming). The full public API surface is preserved in `GameOS/include/gameos.hpp` and `GameOS/include/videoplayback.hpp` as declarations with zero implementations — Microsoft stripped the `.cpp` side at open-sourcing. In parallel, a game-side layer (`MC2Movie`, `bMovie`, `introMovie`, `pilotVideoTexture`) calls into the missing layer.

---

## Task 1: The Bink-shaped hole

### 1A. Dead GameOS-layer video API (the biggest fossil)

**`GameOS/include/videoplayback.hpp`** — entire file is a fossil. Declares `struct gos_Video` with DirectShow interface pointers (`IMultiMediaStream`, `IDirectDrawMediaStream`, `IDirectDrawStreamSample`, etc.) plus a global `VideoManager` singleton. Zero implementations anywhere:
```cpp
// videoplayback.hpp:9-65
typedef struct gos_Video {
    IMultiMediaStream*          m_pMMStream;
    IMediaStream*               m_pPrimaryVidStream;
    IBasicAudio*                m_pBasicAudio;
    IDirectDrawMediaStream*     m_pDDStream;
    ...
    gosVideo_PlayMode           m_videoStatus, m_videoPlayMode;
    RECT                        m_videoSrcRect, m_videoRect;
    DWORD                       m_texture;
    STREAM_TIME                 m_duration;
    ...
public:
    gos_Video(char* path, bool texture);
    bool Update();
    void Pause(); void Continue(); void Stop();
    void FF(double time);
    void Restore(); void Release();
    void SetLocation(DWORD, DWORD);
};
void VideoManagerInstall();
void VideoManagerUninstall();
void VideoManagerPause(); void VideoManagerContinue();
void VideoManagerRelease(); void VideoManagerRestore();
void VideoManagerUpdate();
void VideoManagerFF(double sec);
```
*Was:* the concrete DirectShow-backed video player. *Now:* declarations only, no bodies, and **zero callers of `VideoManager*` or `gos_Video::*` anywhere in the codebase**.

**`GameOS/include/gameos.hpp:430-488`** — the public gos video API. Declarations with zero implementations and zero callers:
```cpp
// gameos.hpp:433-440
enum gosVideo_Command {
    gosVideo_SeekTime = 1, gosVideo_SetCoords = 2, gosVideo_SetScale = 4,
    gosVideo_Volume = 8, gosVideo_Panning = 16
};
// gameos.hpp:444-452
enum gosVideo_PlayMode {
    gosVideo_PlayOnceHide, gosVideo_PlayOnceHold, gosVideo_Loop,
    gosVideo_Stop, gosVideo_Pause, gosVideo_Continue
};
// gameos.hpp:459-476 — full resource info struct
typedef struct _gosVideo_Info {
    char* lpstrPath;
    gosVideo_PlayMode ePlayMode, ePlayStatus;
    DWORD dwOriginX, dwOriginY;
    float fScaleOfX, fScaleOfY;
    float fDurationSec, fSoFarSec;
    unsigned char* lpData;
    DWORD dwSurfaceWidth, dwSurfaceHeight, dwPitch, dwWidth, dwHeight;
} gosVideo_ResourceInfo;
// gameos.hpp:482-488 — public API
void __stdcall gosVideo_CreateResource( HGOSVIDEO* Handle, char* filename );
void __stdcall gosVideo_CreateResourceAsTexture( HGOSVIDEO* Handle, DWORD* hTex, char* filename );
void __stdcall gosVideo_DestroyResource( HGOSVIDEO* Handle );
void __stdcall gosVideo_GetResourceInfo( HGOSVIDEO Handle, gosVideo_ResourceInfo* gvi );
void __stdcall gosVideo_SetPlayMode( HGOSVIDEO Handle, enum gosVideo_PlayMode gvpm );
void __stdcall gosVideo_Command( HGOSVIDEO Handle, enum gosVideo_Command vc, float x, float y = 0.0f );
```
*Was:* the intended public video-playback API exposed by GameOS. *Now:* completely orphaned — no body, no caller, the header declarations are a pure specification of what GameOS promised.

**`HGOSVIDEO` typedef** at `gameos.hpp:185`:
```cpp
typedef struct gos_Video* HGOSVIDEO;
```
Only two remaining references in game code, both commented out at `missionselectionscreen.h:47-48`:
```cpp
//HGOSVIDEO        video;
//unsigned long    videoTexture;
```
*Was:* the MissionSelectionScreen held an HGOSVIDEO + DWORD output texture handle. The `CreateResourceAsTexture` signature confirms the pairing — video rendered into a gos texture, then drawn with the normal UI quad pipeline.

**`GameOS/include/directx.hpp:186-219`** — wrapper declarations around DirectShow interfaces (`IMultiMediaStream`, `IDirectDrawMediaStream`, `IBasicAudio`, `IMediaControl`, `IMediaPosition`, `IMediaSeeking`, `IStreamSample`). Zero implementations; used only by the dead `gos_Video` struct.
```cpp
// directx.hpp:186-191
HRESULT wSetState( IMultiMediaStream* imms, STREAM_STATE NewState );
HRESULT wGetState( IMultiMediaStream* imms, STREAM_STATE * NewState );
HRESULT wSeek( IMultiMediaStream* imms, STREAM_TIME SeekTime );
HRESULT wGetTime( IMultiMediaStream* imms, STREAM_TIME* SeekTime );
HRESULT wGetDuration( IMultiMediaStream* imms, STREAM_TIME *theTime );
HRESULT wGetMediaStream( IMultiMediaStream* imms, ... );
```
*Confirms:* the original implementation was DirectShow-backed.

### 1B. Live game-side fossils (game code that still calls out to the missing layer via a shim)

**`code/ablmc2.cpp:2655-2668` — `execPlayVideo` ABL script function.** Exposed to mission scripts as `playvideo(fileName)`:
```cpp
void execPlayVideo(void) {
    // Starts playback of video sequence
    //   PARAMS: integer, real
    //   Returns: integer (result)
    char* fileName = ABLi_popCharPtr();
    mission->missionInterface->playMovie(fileName);
    ABLi_pushInteger(0);
}
// Registered at ablmc2.cpp:6770:
ABLi_addFunction("playvideo", false, "C", "i", execPlayVideo);
```
*Currently:* routes into `missionInterface->playMovie(fileName)` → `controlGui.playMovie(fileName)` → `new MP4Player(...)`. Still active.

**`code/ablmc2.cpp:5189-5205` — `execSetMovieMode` ABL function.** Triggers letterbox camera mode (NOT video playback — see note in §1E):
```cpp
void execSetMovieMode (void) {
    // Changes game to movie playback mode. GUI disappears, Commands not allowed, Letterbox
    if (eye)
        eye->setMovieMode();
}
// Registered at ablmc2.cpp:6863-6866:
ABLi_addFunction("setmoviemode", false, NULL, NULL, execSetMovieMode);
ABLi_addFunction("endmoviemode", false, NULL, NULL, execEndMovieMode);
ABLi_addFunction("forcemovieend", false, NULL, "i", execForceMovieEnd);
```

**`code/objective.h:509-525` and `code/objective.cpp:1163-1187` — `CPlayBIK` objective-action class.** Authentic Microsoft-era Bink reference. A mission objective action type (`PLAY_BIK`) that reads a video path from a mission's FitIniFile field:
```cpp
// objective.cpp:1163-1174
bool CPlayBIK::Read( FitIniFile* missionFile ) {
    long result = 0;
    ECharString tmpECStr;
    result = sReadIdString(missionFile, "AVIFileName", tmpECStr);
    ...
    m_pathname = tmpECStr2.Data();
    return true;
}
// objective.cpp:1183-1187
int CPlayBIK::Execute() {
    ControlGui::instance->playMovie( m_pathname );
    return true;
}
```
*Significance:* (1) the FitIniFile field is `AVIFileName`, (2) the class is named `CPlayBIK` ("BIK" = Bink). The naming inconsistency is a fingerprint — mission data shipped with `.bik` files whose references in data files are labeled `AVIFileName`. Enum entry at `objective.h:479`: `PLAY_BIK,`. String-table at `objective.cpp:62`: `"PlayBIK"`. Resource string `IDS_PlayBIK = 51050` at `resource.h:4211`.

**`code/controlgui.h:154-158, 368, 387` — ControlGui movie fields.** The briefing-window video state:
```cpp
// controlgui.h:154-158
void playMovie( const char* fileName );
bool isMoviePlaying();
bool playPilotVideo( MechWarrior* pPilot, char movieCode );
void endPilotVideo();
// controlgui.h:364-368
StaticInfo*      videoInfos;        // border/frame statics around the video
long             videoInfoCount;
GUI_RECT         videoRect;         // destination rect for the video
GUI_RECT         videoTextRect;     // DANGLING — loaded but never used (see below)
MP4Player*       bMovie;
// controlgui.h:387
bool             moviePlaying;
```

**`videoTextRect` — DANGLING field.** Loaded from a "VideoTextBox" block in the button-layout file at `controlgui.cpp:2498-2504`:
```cpp
file.seekBlock( "VideoTextBox" );
file.readIdLong( "left", videoTextRect.left );
...
videoTextRect.left += hiResOffsetX;
videoTextRect.right += hiResOffsetX;
```
But `videoTextRect` is **never read** anywhere else in the codebase. Originally probably overlay/subtitle/caption rect shown during a briefing video. Fossil field.

**`code/mechicon.h:160-161` and `code/mechicon.cpp:33-35` — pilot-portrait video state.**
```cpp
// mechicon.h:160-161
static DWORD         pilotVideoTexture;
static MechWarrior*  pilotVideoPilot;
// mechicon.cpp:33-34
MP4Player*  ForceGroupIcon::bMovie = NULL;
DWORD       ForceGroupIcon::pilotVideoTexture = 0;
```
Two paths coexist (`mechicon.cpp:962-983`): `bMovie` (live video) OR `pilotVideoTexture` (static still image). The `pilotVideoTexture` branch at line 977-982 uses the `gos_SetRenderState(gos_State_Texture, handle) + gos_DrawQuads(v, 4)` pattern — the same pattern the commented-out missionselection block uses. Strong evidence that the *original* video path also produced a DWORD texture handle consumed by `gos_SetRenderState`.

**`code/missionselectionscreen.cpp:89-113` — commented-out original video-rendering code.** Block preserved since repository's initial commit (2016-09-19, pre-open-source):
```cpp
/*
gos_VERTEX v[4];
for( int i = 0; i < 4; i++ ) { ... v[i].argb = 0xffffffff; ... }
v[0].x = rects[VIDEO_RECT].left()+1 + xOffset;
v[0].y = rects[VIDEO_RECT].top()+1 + yOffset;
v[2].x = v[3].x = v[0].x + rects[VIDEO_RECT].width()-2;
v[2].y = v[1].y = v[0].y + rects[VIDEO_RECT].height()-2;
v[2].u = v[3].u = 1.0f; v[2].v = v[1].v = 1.0f;

gos_SetRenderState( gos_State_Texture,  videoTexture );
gos_DrawQuads( v, 4 );
gos_SetRenderState( gos_State_Texture,  0 );
*/
```
*Was:* build a 4-vertex quad at `rects[VIDEO_RECT]`, bind the per-frame-updated `videoTexture` DWORD, draw. *Now:* replaced by `bMovie->render()` at line 116.

**`code/forcegroupbar.cpp:458-459` — another authentic original-API fossil.** Inside `ForceGroupBar::setPilotVideo`, original call signature preserved as comments next to the new MP4Player replacement:
```cpp
//ForceGroupIcon::bMovie = new MC2Movie;
//ForceGroupIcon::bMovie->init(aviPath,vRect,true);
```
*Was:* default-constructed `MC2Movie`, then `init(path, rect, loop)` — the pre-rewrite contract. Note `aviPath` variable name alongside `.bik` extension is a recurring pattern (see §1C).

**`code/backup - not working, not crashing/mainmenu.cpp:173-174`** — the intro-video-create site, identical shape:
```cpp
introMovie = new MC2Movie;
introMovie->init(path,movieRect,true);
```
and the matching update loop at `:469`:
```cpp
bool result = introMovie->update();
if (result) {
    //Movie's Over. Whack it.
    delete introMovie;
    introMovie = NULL;
}
```
*Contract:* `update()` returns `bool` where `true` means "done, delete me" — this is the old shape of the API. MP4Player changed it to `void update()` + separate `isPlaying()` check. The backup directory captures a transitional shape (`MC2Movie`-keyed, but path handling already `.mp4`-aware) from before the rewrite to `MP4Player`.

### 1C. File path / format strings referencing old video formats

All commented-out or legacy references. Grep comprehensive:
- `code/forcegroupbar.cpp:445-447` — `FullPathFileName aviPath; aviPath.init(moviePath, pVideo, ".mp4"); //aviPath.init(moviePath, pVideo, ".bik");`
- `code/logistics.cpp:292-293` — `//path.init(moviePath, "credits", ".bik");` then `movieName.init(moviePath, "credits", ".mp4");`
- `code/logistics.cpp:860-861` — `//movieName.init(moviePath, fileName, ".bik");` then `movieName.init(moviePath, fileName, ".mp4");`
- `code/missionselectionscreen.cpp:237` — `//videoName.init( moviePath, str, ".bik" );`
- `code/backup - not working, not crashing/mainmenu.cpp:164` — `//path.init( moviePath, "msft", ".bik" );`
- `code/controlgui.cpp:2869` comment — `// Assume extension is BIK.`
- `code/controlgui.cpp:3204` comment — `// stupid name format = "Worm 1 filter.avi"` (original pilot-video naming)
- `code/radio.cpp:276` comment — `// the smacker window. It wouldn't leak from the RadioHeap`
- `mclib/abldbug.cpp:1237` comment — `// Some assumptions: this will never be called during a smacker movie.`

**Interpretation:** `.bik` references outnumber `.smk` references 6:2. The two `smacker` references are both in *comments about other systems* (radio, debugger) warning about interactions with the video window — they don't prove Smacker was the actual format. The Bink/AVI naming mix (`aviPath` variable holding a `.bik` path) plus the `CPlayBIK` class reading an `AVIFileName` field suggests Bink-encoded content played through DirectShow filters, with "AVI" used historically as a generic "video file" term. **Cannot tell with certainty** whether the shipped videos were literally `.bik` Bink or `.avi` with a codec — but the code called them `.bik` and the class was `CPlayBIK`.

### 1D. LogisticsMissionInfo: "FinalVideo" campaign data

`code/logisticsmissioninfo.cpp:110-118`:
```cpp
char tmp[256];
if ( NO_ERR == file.readIdString( "FinalVideo", tmp, 255 ) ) {
    finalVideoName = tmp;
}
```
Mission data files contain a `FinalVideo=...` field. `LogisticsData::getFinalVideo()` returns it (`logisticsdata.cpp:2640-2642`). Played by `Logistics::update()` at `logistics.cpp:152` via `playFullScreenVideo(...)` when `campaignOver()` triggers. Still live.

### 1E. Related-but-not-video fossils (mentioning for completeness)

**`mclib/camera.h:273-274, 724-757` — `setMovieMode` / `endMovieMode` / `forceMovieEnd`.** These are **camera letterbox** controls, not video playback. `setMovieMode()` at `:724` just animates black bars on/off for scripted cinematic camera shots:
```cpp
// camera.h:724-734
void setMovieMode (void) {
    if (!inMovieMode) {
        inMovieMode = true;
        startEnding = false;
        forceMovieEnd = false;
        letterBoxPos = 0.0f;     //Start letterboxing;
        letterBoxTime = 0.0f;
    }
}
void endMovieMode (void) {
    if (inMovieMode)
        startEnding = true;      //Start the letterbox shrinking.  When fully shrunk, inMovieMode goes false!
}
```
Name overlap is confusing. Not a video-playback fossil.

**`code/backup - not working, not crashing/mp4player.cpp:141`** — `// match game's cinematic frame rate` + `SDL_Delay(1000/15)`. Implies the original cinematic FPS was 15 (matches CINEMA*.mp4 currently shipping at 15 FPS).

**`code/logistics.cpp:282-284`** — string-compare on `"cinema5"`:
```cpp
//Check if we are cinema5.  If we are, spool up the credits and play those.
if (S_strnicmp(bMovie->getMovieName().c_str(), "cinema5", 7) == 0)
```
Hard-coded sequence: after CINEMA5, play credits. Confirms there was a fixed cinema1..cinema5 storyline chain in the campaign flow.

**`code/mc2movie.h`** (archived) — **not an original fossil.** This file was Alex's 2025 rewrite; contains SDL2, GLEW, `std::atomic`, `std::chrono`. The original MC2Movie class did not survive into the open-source drop — only its *shape* survives in call sites (`new MC2Movie` + `init(path,rect,loop)` + `update()`→bool + `stop()` + `render()`).

### 1F. What's NOT in the codebase (confirmed absences)

- No `gos_Video::*` method bodies anywhere.
- No `VideoManager*` function bodies or callers.
- No `gosVideo_*` function bodies or callers (other than the declarations).
- No `class MC2Movie` in any file rooted in the repository's original tree. Alex's 2025 `archive/fmv-dead-code/code/mc2movie.{h,cpp}` is distinct — it includes SDL2 and std::atomic which did not exist in the 2001 codebase.
- No BINK SDK headers, no `Bink*` function references beyond comments/class-name suffixes, no `SmackOpen*` references, no `RAD*` anything.

---

## Task 2: Enumerated video API contract

Verbs reconstructed from (a) call-site expectations in current game code and (b) fossil declarations. "Still expected" means a live call site currently exists and drives the verb via MP4Player.

| Verb | Signature (fossil / current) | Call sites still expecting it | Status |
|---|---|---|---|
| **open(path)** | `new MC2Movie` default-ctor + `init(path, rect, loop)` (fossil, `forcegroupbar.cpp:458-459`, `backup mainmenu.cpp:173-174`) / `new MP4Player(path, window, ctx)` + `init(path, rect, loop, window, ctx)` (current, `controlgui.cpp:2887`, `logistics.cpp:303`, `logistics.cpp:871`, `mainmenu.cpp:185`, `missionselectionscreen.cpp:263`) | Every video-playing screen | Required, live |
| **close / destroy** | `delete bMovie; bMovie = NULL;` | `controlgui.cpp:271, 950, 2921`; `forcegroupbar.cpp:419-420`; `mainmenu.cpp:419, 461`; `missionselectionscreen.cpp:46, 257`; fossil: `gosVideo_DestroyResource(HGOSVIDEO*)` | Required, live |
| **render / draw** | `bMovie->render()` | `controlgui.cpp:371`; `logistics.cpp:431`; `mainmenu.cpp:455, 724`; `mechicon.cpp:974`; `missionselectionscreen.cpp:116` | Required, live |
| **update / tick** | `bool bMovie->update()` (old contract: returns true when done) / `void update()` (new) | `controlgui.cpp:946, 2911`; `logistics.cpp:274`; `mainmenu.cpp:454`; `missionselectionscreen.cpp:141`; fossil: `gos_Video::Update() → bool` | Required, live. **Contract mismatch:** original returned bool (EOF signal), current returns void + `isPlaying()` side query. |
| **isPlaying / isFinished** | `bool isPlaying()` | `controlgui.cpp:947, 2912`; `forcegroupbar.cpp:535`; `mainmenu.cpp:458` | Required, live (substitutes for original's bool-returning update) |
| **stop** | `stop()` | `logistics.cpp:271`; `mainmenu.cpp:418`; `mechicon.cpp:966`; `missionselectionscreen.cpp:136, 141, 256, 269, 273, 336`; fossil: `gos_Video::Stop()`, `gosVideo_PlayMode::gosVideo_Stop` | Required, live |
| **pause / resume** | `pause()` | No live call sites in game code. MP4Player has `pause()` (`mp4player.h:34`) but nobody calls it. Fossil: `gos_Video::Pause()`, `gos_Video::Continue()`, `VideoManagerPause()`, `VideoManagerContinue()`, `gosVideo_Pause`, `gosVideo_Continue` | Declared but unused — original system expected it (e.g. game pause menu during a video); current codebase has no pause-during-video flow. Low priority. |
| **restart / seek-to-start** | `restart()` | `mainmenu.cpp:192`; `mp4player.h:34` | Live |
| **setRect** | `setRect(RECT)` | `mp4player.h:38` + `mp4player.cpp:600` defined; ONE caller site was intended at `mechicon.cpp:973`: `// bMovie->setRect(vRect); // Removed as MC2Movie has no member setRect` — comment notes the *original* class lacked it. **The call site that wants it is live** (pilot portraits shift when icons re-arrange on death) but has been neutered. | Required for pilot portraits; currently neutered. |
| **setLooping** | Taken at construction via `init(...,loop,...)`. No post-construction mutator. Fossil: `gosVideo_SetPlayMode(handle, gosVideo_Loop)` | `gosVideo_Loop` enum value suggests original could change loop state mid-playback; no live need for it today. | Encoded in init only |
| **setVolume** | `setVolume(float)` | `mp4player.h:37` declared, `mp4player.cpp:596` has empty body (`// SDL2 doesn't have per-device volume; would need to scale in callback`). **Zero callers in game code.** Fossil: `gos_Video::m_volume` field, `gosVideo_Volume` enum | Declared but unused in game; original had it. |
| **setPanning** | None | No callers. Fossil: `gos_Video::m_panning`, `gosVideo_Panning` enum | Original only |
| **skipToEnd / seek** | None in MP4Player. Fossil: `gos_Video::FF(double time)`, `VideoManagerFF(double sec)`, `gosVideo_SeekTime` enum. `CPlayBIK::Execute()` does NOT seek. | Original had it. No live caller needs it today. | Original only |
| **getCurrentFrame / getProgress** | `getFrameCount()` declared at `mp4player.h:41`. Fossil: `gos_Video::m_lastKnownTime`, `gosVideo_ResourceInfo::fSoFarSec` / `fDurationSec` | No live caller of either | Declared, unused |
| **getDuration** | None in MP4Player. Fossil: `gos_Video::m_duration`, `gosVideo_ResourceInfo::fDurationSec` | No live caller | Original only |
| **getMovieName / path accessor** | `getMovieName() const` | `logistics.cpp:277, 284` uses it for string-compare to detect "cinema5" → credits transition | Required, live |
| **create-as-texture** | Fossil: `gosVideo_CreateResourceAsTexture(HGOSVIDEO*, DWORD* hTex, char* filename)` + commented pair `//HGOSVIDEO video; //unsigned long videoTexture;` in `missionselectionscreen.h:47-48`, plus the commented `gos_SetRenderState(gos_State_Texture, videoTexture); gos_DrawQuads` block at `missionselectionscreen.cpp:110-111` | Call sites that consumed `videoTexture` have been gutted; **pattern still active** for the static `pilotVideoTexture` case at `mechicon.cpp:979` and for the FMV_RENDERING_RECON.md refactor | Original explicitly supported "render to a texture the game then draws" |
| **setLocation(x,y)** | Fossil: `gos_Video::SetLocation(DWORD, DWORD)`, `gosVideo_SetCoords` command | No live caller (post-init rect move) | Original only |
| **setScale** | Fossil: `gosVideo_SetScale` command, `gos_Video::m_scaleX/m_scaleY` fields | No live caller | Original only |
| **restore / release** | Fossil: `gos_Video::Restore()`, `gos_Video::Release()`, `VideoManagerRelease/Restore` | DirectDraw surface-lost handling. Not relevant to SDL/GL. | Obsolete — OS-specific |
| **playMovie (controlgui)** | `ControlGui::playMovie(const char* fileName)` → routes to MP4Player | `objective.cpp:1185` (`CPlayBIK::Execute`); `missiongui.h:228` (`MissionInterfaceManager::playMovie`); `ablmc2.cpp:2665` (`execPlayVideo`) | Live — this is the ABL/mission entrypoint |
| **playPilotVideo** | `ControlGui::playPilotVideo(MechWarrior*, char movieCode)` | `gamesound.cpp:492` when radio message has a `movieCode` flag | Live — triggers per-pilot portrait video |
| **endPilotVideo** | `ControlGui::endPilotVideo()` | `gamesound.cpp:464` when a pilot message finishes | Live |
| **isMoviePlaying / isPlayingVideo** | `ControlGui::isMoviePlaying()` (briefing); `ForceGroupBar::isPlayingVideo()` (pilot portrait) | `forcegroupbar.cpp:430-431` gates pilot-video start on "is anything else already playing" | Live — enforces only-one-video-at-a-time |
| **playFullScreenVideo** | `Logistics::playFullScreenVideo(const char*)` at `logistics.cpp:854-877` | `logistics.cpp:152` (campaign end); `mainmenu.cpp` intro sequence also goes fullscreen | Live |

### Ambient verbs (video-mode toggling, not playback)

| Verb | Purpose | Callers |
|---|---|---|
| `setMovieMode` / `endMovieMode` / `forceMovieToEnd` | Camera letterbox toggle (NOT video playback itself) | `ablmc2.cpp:5204, 5224` ABL bindings; ABL scripts expected to toggle before/after scripted cinematic sequences |
| `inMovieMode` flag | Game loop sentinel: disables HUD, disables input, enables letterbox | `camera.h:273`, queried at `controlgui.cpp:956`, `gamecam.cpp:165,188,201`, `missiongui.cpp:430,432` |
| `forceMovieEnd` flag | Set by `forceMovieToEnd`. ABL script polls via `forcemovieend` and calls `endMovieMode` voluntarily. | `ablmc2.cpp:5304` (queried) |

### Spec a working video layer must satisfy

Minimum call-site compatibility contract (everything below has at least one live caller):
1. **Constructor + `init(path, rect, loop, window, ctx)`.** Load a file into a ready state given a destination rect and loop flag.
2. **`update()`** — demux/decode tick. Old contract returned bool (done). New contract returns void; `isPlaying()` is queried separately.
3. **`render()`** — draw current frame into `displayRect` (or — per the gosTexture precedent — produce a texture handle the caller draws).
4. **`stop()`** — immediate termination.
5. **`isPlaying()`** — playback state query; false when done.
6. **`restart()`** — rewind to start.
7. **`getMovieName() const`** — retrieve the original path, used for the "cinema5 → credits" transition.
8. **`setRect(RECT)`** — required by `mechicon.cpp:973` (neutered call site); pilot portraits shift when icons rearrange.
9. Game-side routing: `ControlGui::playMovie / isMoviePlaying / playPilotVideo / endPilotVideo`; `ForceGroupBar::setPilotVideo / isPlayingVideo`; `Logistics::playFullScreenVideo`.

Optional (fossil-only, no live caller but original system supported them):
- `pause()` / `resume()` — declared in MP4Player, unused.
- `setVolume(float)` — declared and empty-bodied in MP4Player.
- `seek(time)` — original `FF(double)` / `gosVideo_SeekTime`. No live caller.
- `setLocation` / `setScale` / `setPanning` — original fine-grained controls. No live caller.

---

## Task 3: Texture size handling in `createHardwareTexture`

**Answer: arbitrary W×H. No POT assertions, no size snapping, no rejection.**

Trace end-to-end:

1. `gos_NewEmptyTexture(format, name, HeightWidth, hints, ...)` at `gameos_graphics.cpp:2334-2351`:
```cpp
int w = HeightWidth;
int h = HeightWidth;
if(HeightWidth & 0xffff0000) {
    h = HeightWidth >> 16;
    w = HeightWidth & 0xffff;
}
gosTexture* ptex = new gosTexture(Format, Hints, w, h, Name);
if(!ptex->createHardwareTexture()) {
    STOP(("Failed to create texture\n"));
    return INVALID_TEXTURE_ID;
}
return g_gos_renderer->addTexture(ptex);
```
Supports packed W|H via `RECT_TEX(w,h)` or square via just `HeightWidth`. No size check before calling `createHardwareTexture`.

2. `gosTexture::createHardwareTexture()` at `gameos_graphics.cpp:865-924`. The empty-texture branch (lines 912-922) is the one relevant to video:
```cpp
} else {
    gosASSERT(tex_.w >0 && tex_.h > 0);   // only asserts non-zero
    TexFormat tf = TF_RGBA8;
    DWORD* pdata = new DWORD[tex_.w*tex_.h];
    for(int i=0;i<tex_.w*tex_.h;++i)
        pdata[i] = 0xFF00FFFF;
    tex_ = create2DTexture(tex_.w, tex_.h, tf, (const uint8_t*)pdata);
    delete[] pdata;
    return tex_.isValid();
}
```
Only assertion is `w>0 && h>0`. No POT check, no max-size check.

3. `create2DTexture(int w, int h, TexFormat fmt, const uint8_t* texdata)` at `GameOS/gameos/utils/gl_utils.cpp:165-190`:
```cpp
GLuint texID;
glGenTextures(1, &texID);
glBindTexture(GL_TEXTURE_2D, texID);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexImage2D(GL_TEXTURE_2D, 0, textureInternalFormats[fmt],
        w, h, 0, textureFormats[fmt], textureFormatChannelType[fmt], texdata);
```
`w` and `h` are passed through unmodified to `glTexImage2D`. No snap. No size validation.

Grep for POT / size-rounding in GameOS/ returned **zero matches** for `power.*of.*two`, `POT`, `powerOfTwo`, `snap.*size`, `roundUp.*2`, or `isPowerOfTwo`. The "512, 256, 128, 64, 32 or 16*16" restriction mentioned in the `gos_NewEmptyTexture` doc comment (`gameos.hpp:2807`) is **stale documentation** — the modern OpenGL backend doesn't enforce it.

**Conclusion:** 1920×1080 works. 76×54 works. Any `w,h > 0` works. Only runtime constraint is OpenGL's `GL_MAX_TEXTURE_SIZE` (typically 16384 or 32768 on modern hardware — far above anything needed). One minor wrinkle: `GL_TEXTURE_MIN_FILTER = GL_NEAREST` is hardcoded in `create2DTexture` — no mipmaps generated, which is fine for video.

---

## Task 4: Git blame on two specific sites

### `missionselectionscreen.cpp:99-112` (commented-out gos_DrawQuads video block)

```
^305ad87 (sebi 2016-09-19 21:53:48 +0300  90) 		gos_VERTEX v[4];
...
^305ad87 (sebi 2016-09-19 21:53:48 +0300 110) 		gos_SetRenderState( gos_State_Texture,  videoTexture );
^305ad87 (sebi 2016-09-19 21:53:48 +0300 111) 		gos_DrawQuads( v, 4 );
^305ad87 (sebi 2016-09-19 21:53:48 +0300 112) 		gos_SetRenderState( gos_State_Texture,  0 );
^305ad87 (sebi 2016-09-19 21:53:48 +0300 113) 		*/
```

Commit `305ad87` — "initial commit" by sebi, 2016-09-19. That's the repository's **root commit**, the Microsoft source-release dump. The `^` prefix indicates the blame could not look further back because it hit the root.

**Interpretation:** this block was commented out before the code was open-sourced. Microsoft shipped it already commented. The commit message ("initial commit") gives no further context.

### `mechicon.cpp:973` (`// bMovie->setRect(vRect); // Removed as MC2Movie has no member setRect`)

```
^305ad87 (sebi                    2016-09-19 21:53:48 +0300 970) 			}
^305ad87 (sebi                    2016-09-19 21:53:48 +0300 971) 			else
^305ad87 (sebi                    2016-09-19 21:53:48 +0300 972) 			{
859c7e77 (Alexandros Mandravillis 2025-04-08 23:19:59 +0300 973) 				// bMovie->setRect(vRect); // Removed as MC2Movie has no member setRect
^305ad87 (sebi                    2016-09-19 21:53:48 +0300 974) 				bMovie->render();
^305ad87 (sebi                    2016-09-19 21:53:48 +0300 975) 				}
```

Commit `859c7e77` — "big rework done, troubleshooting video playback" by Alexandros Mandravillis, 2025-04-08.

**Interpretation:** this is NOT a Bink-era fossil. Alexandros commented the line out in 2025 with an explicit note that the current `MC2Movie` class lacked `setRect`. The original line either (a) was `bMovie->setRect(vRect);` at some prior revision referencing some other movie class that did support setRect, or (b) was Alex's own addition that he then noticed wouldn't compile. Git blame can't distinguish — the *preceding* state of that specific line didn't exist in the root commit (lines 970, 972, 974 are from 2016-09-19, but 973 shows Alex's blame, meaning line 973 was inserted in 2025, not modified from a 2016 version). So this is Alex adding-then-commenting, not a true fossil. **The intent behind wanting setRect is the fossil — the comment records that the call *would be needed* for per-frame rect updates (pilot icons rearrange as units die) but the current player does not support it in a way that was wired up.**

---

## Callouts / unresolved

- **Bink vs DirectShow.** The naming (`CPlayBIK`, `.bik` extensions, `aviPath` variable) points at Bink files. The GameOS fossils (`IMultiMediaStream`, DirectShow wrappers in `directx.hpp`) point at a DirectShow-backed player. **Most plausible reconciliation** (but cannot confirm from code alone): shipped assets were Bink, played through a DirectShow filter. Alternatively the assets were AVI with Cinepak/Indeo/similar, and "BIK" is a misnomer in the class name. Needs external confirmation from someone who owns original game media.
- **`MC2Movie` original shape.** The original class header/impl did not survive. The only evidence of its shape is the `new MC2Movie; init(path,rect,loop); update()→bool; stop(); render();` pattern preserved in commented code at `forcegroupbar.cpp:458-459` and in the pre-rewrite backup `code/backup - not working, not crashing/mainmenu.cpp`. Whether MC2Movie was game-side and called into `gos_Video`, or whether it replaced `gos_Video` entirely at some point, cannot be determined.
- **`videoTextRect`.** Field is loaded from button-layout files (`controlgui.cpp:2498-2504`) but never read. Might have been subtitle/caption positioning. No surviving code that rendered text into it.
- **`setRect` on pilot portrait.** Live call site at `mechicon.cpp:973` that was neutered. Unclear whether this is a gap a replacement MUST close or an optimization that can be skipped (pilot icons *can* rearrange mid-game on unit death).
- **`movieCode` character field in radio messages** (`radio.cpp:348, 172`). Each radio message carries a single-char `movieCode` used to select a pilot video variant (seen in `controlgui.cpp:3216-3219` where `movieCode` is concatenated to the pilot name: e.g. "bubba" + "1" → "bubba1.mp4"). Part of the live contract, though the character-letter encoding is itself a legacy artifact.
