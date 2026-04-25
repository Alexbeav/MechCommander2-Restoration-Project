# FMV Reference Notes

Architectural notes extracted from three open-source engines that solved
FMV-replacement problems similar to MC2's. Input-gathering only — no design
proposal, no comparison to MC2's current code.

All citations are relative to `repo-references/` in the MC2 working tree.
File:line references are good enough to jump to source.

- OpenMW — FFmpeg-based standalone player (`openmw/extern/osg-ffmpeg-videoplayer/`)
- RBDOOM-3-BFG — engine-integrated `idCinematic` with FFmpeg / libbinkdec backends (`RBDOOM-3-BFG/neo/renderer/Cinematic.{h,cpp}`)
- GemRB — plugin-registered `BIKPlayer` (`gemrb/gemrb/plugins/BIKPlayer/`)

---

## Section 1 — OpenMW's video player API surface

Location: `openmw/extern/osg-ffmpeg-videoplayer/`. The module is self-contained
and meant to be reusable (licensed separately from OpenMW itself).

### 1.1 Public API of VideoPlayer

Declared in `include/osg-ffmpeg-videoplayer/videoplayer.hpp`. The class wraps
a `std::unique_ptr<VideoState>` and mostly forwards.

| Method | Decl | Purpose |
|--------|------|---------|
| `VideoPlayer()` | videoplayer.hpp:30 | Ctor; `mState = nullptr` until `playVideo()` |
| `~VideoPlayer()` | videoplayer.hpp:31 | Dtor; calls `close()` if state exists |
| `setAudioFactory(MovieAudioFactory*)` | videoplayer.hpp:37 | Register audio output handler; takes ownership. Called before `playVideo()` |
| `bool hasAudioStream()` | videoplayer.hpp:40 | True if the currently open video has an audio stream |
| `void playVideo(std::unique_ptr<std::istream>&&, const std::string& name)` | videoplayer.hpp:45 | Open and start playing from a stream; closes any prior playback |
| `double getCurrentTime()` | videoplayer.hpp:48 | Current playhead via `VideoState::get_master_clock()` |
| `double getDuration()` | videoplayer.hpp:50 | Total duration |
| `void seek(double time)` | videoplayer.hpp:54 | Request seek; delegates to `VideoState::seekTo()` |
| `void play() / pause() / bool isPaused()` | videoplayer.hpp:56-58 | Playback control; forwards to `VideoState::setPaused()` |
| `bool update()` | videoplayer.hpp:62 | **Per-frame tick from the host render loop.** Returns `false` when the video has ended |
| `void close()` | videoplayer.hpp:65 | Stop playback and free state |
| `osg::ref_ptr<osg::Texture2D> getVideoTexture()` | videoplayer.hpp:68 | The texture with the latest decoded frame. Called once at start, re-read each frame |
| `int getVideoWidth() / getVideoHeight()` | videoplayer.hpp:71-72 | Source video dimensions (0 if no texture yet) |

Everything else (packet queues, decode threads, PTS math) is private inside
`VideoState`.

### 1.2 End-to-end usage sequence

OpenMW itself drives this from `apps/openmw/mwgui/videowidget.cpp` and
`windowmanagerimp.cpp`. The flow:

1. **Setup.** Create `VideoPlayer` (owned as `std::unique_ptr`). Call
   `setAudioFactory(new MWSound::MovieAudioFactory())` to wire audio output
   (videowidget.cpp:34).
2. **Open.** Resolve the file through the VFS to get an `std::istream`, then
   `playVideo(std::move(stream), name)` (videowidget.cpp:47). Internally:
   - `VideoPlayer::playVideo` closes any prior state, constructs a new
     `VideoState`, attaches the audio factory, calls `mState->init(...)`
     (videoplayer.cpp:31-46).
   - `VideoState::init` builds an `AVIOContext` wrapping the istream, opens
     the format, finds streams, spawns a **ParseThread** that reads packets
     (videostate.cpp:737-815). Stream opens internally spawn a **VideoThread**.
   - `playVideo` then spin-loops `mState->update()` until the first texture
     is populated, so the caller never sees an empty first frame
     (videoplayer.cpp:42-46).
3. **Get texture once.** `getVideoTexture()` → returns a stable `osg::Texture2D`
   that the renderer binds into the scene graph (videowidget.cpp:49-57). The
   *pointer* is stable; its *contents* are what change frame-to-frame.
4. **Per-frame pump.** Main render loop:

   ```cpp
   while (mVideoWidget->update()) {
       // draw frame; texture is already updated
   }
   ```

   (windowmanagerimp.cpp:2089). `update()` → `VideoState::update()` →
   `video_refresh()` which may pop a ready frame off the picture queue and
   upload it to the texture (videostate.cpp:313-359).
5. **Cleanup.** `close()` → `VideoState::deinit()` flushes packet queues,
   joins threads, frees codec contexts, clears the picture queue
   (videoplayer.cpp:84-93).

Key property: the public surface is tiny and entirely synchronous from the
caller's perspective. All the concurrency lives inside.

### 1.3 Frame path: decode → screen

The data path is layered:

```
File / istream
    │
    ▼  AVIOContext wraps istream ops
 ParseThread (videostate.cpp:507-647)
    │  av_read_frame() — one AVPacket at a time
    │  routes by stream index
    ├──►  audioq  (PacketQueue, cap 5 * 16 KB)
    └──►  videoq  (PacketQueue, cap 5 * 256 KB)
           │
           ▼
       VideoThread (videostate.cpp:432-505)
           │  avcodec_send_packet / avcodec_receive_frame
           │  synchronize_video() stamps PTS
           │  queue_picture()          ◄── videostate.cpp:362-410
           │     - sws_scale YUV→RGBA into vp->rgbaFrame->data[0]
           │     - write to circular pictq[] at pictq_windex
           │     - signals pictq_cond
           ▼
       pictq[]  (size VIDEO_PICTURE_QUEUE_SIZE + 1; one "extra" slot to
                 avoid overwriting the frame the renderer is reading)
           │
           ▼
   Main thread: VideoState::video_refresh() (videostate.cpp:313-359)
      - reads master clock
      - if pictq[rindex].pts > clock + 0.03: not yet — redraw previous
      - if pictq[rindex].pts + 0.03 < clock: too late — skip ahead
      - otherwise call video_display(vp)                ◄── :291-311
           - lazily create osg::Texture2D once
           - wrap vp->rgbaFrame->data[0] in an osg::Image
             with osg::Image::NO_DELETE (no copy, no free)
           - texture->setImage(image)
      - advance pictq_rindex
           │
           ▼
   OSG render: binds the Texture2D during scene draw
```

Critical detail: **no copy from FFmpeg into a texture upload buffer.** The
`osg::Image` points *directly* at the RGBA frame in the picture queue slot.
The `+1` slot in the ring buffer is specifically to keep that slot alive
while the GPU is sampling from it. That's also why `VIDEO_PICTURE_QUEUE_SIZE`
is as high as 50 (videostate.hpp:45, 194) — a very large ring.

The RGBA buffer itself is allocated per-slot on first frame (or on dimension
change) via `av_image_alloc()` inside `VideoPicture::set_dimensions`
(videostate.cpp:216, :399). Queue-slot memory is owned by `VideoState` and
only freed in `deinit()`.

### 1.4 Audio sync & master clock

Master clock selection (videostate.cpp:886-893, `get_master_clock`):

- `AV_SYNC_AUDIO_MASTER` (**default**) — returns `get_audio_clock()` which
  reads `mAudioDecoder->getAudioClock()` (audiodecoder.hpp:56 tracks the
  consumed sample position).
- `AV_SYNC_VIDEO_MASTER` — returns `frame_last_pts`.
- `AV_SYNC_EXTERNAL_CLOCK` — wall clock via `ExternalClock::get()`
  (videostate.cpp:883).

Video-side sync (`video_refresh`, videostate.cpp:313-359):

- **30 ms threshold.** Constant `0.03f` in the PTS comparisons. A frame
  whose PTS is more than 30 ms past the master clock is discarded; a frame
  more than 30 ms ahead is deferred.
- When the decoder falls behind, the refresh loop advances through the
  ring skipping slots that are already too old (:337-344), draining the
  queue quickly. Otherwise it redraws the previous frame.

Audio-side sync (`audiodecoder.cpp:78` ctor, `synchronize_audio`):

- `mAudioDiffThreshold = 2.0 * 0.050 = 0.1` seconds — only correct drift
  > 100 ms.
- `mAudioDiffAvgCoef = exp(log(0.01 / AUDIO_DIFF_AVG_NB))` — an
  exponentially weighted moving average of drift over ~20 samples, so
  single-frame jitter doesn't trigger resampling.
- Correction: resample, or duplicate/drop samples.

Hard-won hint (videostate.cpp:564-565, around the seek logic):

> AVSEEK_FLAG_BACKWARD appears to be needed, otherwise ffmpeg may seek to
> a keyframe *after* the given time

Seek clears `audioq`, `videoq`, injects a flush sentinel packet with the
new PTS, resets `pictq` read/write indices, and bumps `ExternalClock`
(videostate.cpp:537-600).

### 1.5 Threading model

Four interacting execution contexts:

| Thread | Role | Lives in |
|--------|------|----------|
| **Main** | Calls `VideoPlayer::update()` from the host render loop; reads `pictq` under mutex in `video_refresh`; owns the `osg::Texture2D` lifetime | app code |
| **ParseThread** | `av_read_frame()` loop; feeds videoq/audioq; honours `mSeekRequested`; rate-limits with 10 ms sleeps when queues are full | videostate.cpp:507-647, spawned :814 |
| **VideoThread** | Drains videoq; decodes via avcodec; `queue_picture()` into `pictq` | videostate.cpp:432-505, spawned :727 via `stream_open` |
| **AudioThread** | Owned by the audio decoder; consumed through `MovieAudioFactory`; drives the audio clock | audiodecoder.{h,cpp} |

Shared state & synchronization:

| Structure | Protection | Where |
|-----------|------------|-------|
| `PacketQueue` (videoq, audioq) | mutex + condvar; also an "abort" flag | videostate.hpp:88-108 |
| `pictq[]` | `pictq_mutex` + `pictq_cond` | videostate.hpp:197-198 |
| `mSeekRequested`, `mSeekPos` | `std::atomic<bool>/atomic<uint64>` | videostate.hpp:203-204 |
| `mVideoEnded`, `mPaused`, `mQuit` | `std::atomic<bool>` | videostate.hpp:206-208 |
| `ExternalClock` | internal `mMutex`, held across get/set | videostate.hpp:66-79, impl :948-962 |

Queue sizes (videostate.cpp:50-51):

- `MAX_AUDIOQ_SIZE = 5 * 16 * 1024` → 80 KB audio
- `MAX_VIDEOQ_SIZE = 5 * 256 * 1024` → 1.25 MB video
- ParseThread throttles itself with `sleep(10)` when either queue is full
  (:603-608). There is **no** decode-side frame sleep — video pacing is
  entirely PTS-driven by the main thread in `video_refresh`.

Picture ring: `VIDEO_PICTURE_QUEUE_SIZE = 50` (videostate.hpp:45), array
size +1 (`pictq.size() = VIDEO_PICTURE_QUEUE_SIZE + 1`, :194). Decoder
advances `pictq_windex`; main thread advances `pictq_rindex`.

---

## Section 2 — RBDOOM's idCinematic interface

Location: `RBDOOM-3-BFG/neo/renderer/Cinematic.{h,cpp}`. Companion audio
abstraction at `neo/sound/CinematicAudio.h`. This is the most directly
relevant reference — `idCinematic` is exactly the shape of an
engine-integrated cinematic module.

### 2.1 idCinematic public API (abstract base)

Declared in `Cinematic.h:70-111`. All methods virtual unless noted.

| Method | Purpose |
|--------|---------|
| `static void InitCinematic()` | Initialize global cinematic state |
| `static void ShutdownCinematic()` | Tear down global state |
| `static idCinematic* Alloc()` | Factory; returns `new idCinematicLocal` (Cinematic.cpp:323) |
| `virtual ~idCinematic()` | Calls `Close()` |
| `virtual bool InitFromFile(const char* qpath, bool looping, nvrhi::ICommandList*)` | Open the file. Supports `.roq`, `.bik`, `.mp4`, `.webm` |
| `virtual cinData_t ImageForTime(int milliseconds, nvrhi::ICommandList*)` | **The frame-fetch pattern.** See §2.4 |
| `virtual int AnimationLength()` | Duration in ms |
| `virtual bool IsPlaying() const` | `status != FMV_EOF` |
| `virtual void Close()` | Stop and free |
| `virtual void ResetTime(int time)` | Seek; `status = FMV_PLAY` |
| `virtual int GetStartTime()` | When playback began |
| `virtual void ExportToTGA(bool skipExisting)` | Debug / frame-dump utility |
| `virtual float GetFrameRate() const` | Source fps (default 30.0) |

Notable: `ImageForTime` and `InitFromFile` both take an `nvrhi::ICommandList*`
because RBDOOM's post-2022 rewrite uses NVRHI for GPU upload — the command
list is the render-graph context into which decoded frames are uploaded as
textures.

### 2.2 idCinematicLocal

The concrete implementation lives in `Cinematic.cpp:92-157`. It extends
`idCinematic` with backend-specific methods conditionally compiled on
`USE_FFMPEG` / `USE_BINKDEC` (mutually exclusive — see §2.6):

```cpp
#if defined(USE_FFMPEG)
    cinData_t ImageForTimeFFMPEG(int ms, nvrhi::ICommandList* cl);
    bool InitFromFFMPEGFile(const char* qpath, bool looping, nvrhi::ICommandList*);
    void FFMPEGReset();
#endif

#ifdef USE_BINKDEC
    cinData_t ImageForTimeBinkDec(int ms, nvrhi::ICommandList* cl);
    bool InitFromBinkDecFile(const char* qpath, bool looping, nvrhi::ICommandList*);
    void BinkDecReset();
#endif
```

Lifecycle state:

- `idFile* iFile` — the file handle
- `cinStatus_t status` — state enum (see §2.3)
- `int startTime` — wall-clock ms at which `ResetTime` was called; all
  frame-for-time math is relative to this
- `bool looping`
- `bool isRoQ` — legacy RoQ path (id's pre-Bink format). If false, the
  frame fetch dispatches to one of the two new backends; if true, it
  falls through to the original RoQ code path

### 2.3 State machine

Enum from `Cinematic.h:45-54`:

```c
typedef enum {
    FMV_IDLE,
    FMV_PLAY,
    FMV_EOF,
    FMV_ID_BLT,      // legacy / internal
    FMV_ID_IDLE,     // legacy / internal
    FMV_LOOPED,
    FMV_ID_WAIT      // legacy / internal
} cinStatus_t;
```

Transitions (from Cinematic.cpp):

- `IDLE → PLAY` via `ResetTime(t)`: sets `startTime = t`, `status = FMV_PLAY`
  (:1251-1254).
- `PLAY → LOOPED` when decoder reports end-of-stream; if `looping` is set,
  status is immediately flipped back to `PLAY` with `startTime = thisTime`
  (:1340-1349). If `!looping`, `LOOPED → IDLE` (:1354).
- `PLAY → EOF` on decode error or non-looping end.
- Host check: `ImageForTime` returns a zeroed `cinData_t` if
  `status == FMV_EOF || status == FMV_IDLE` (:1273).

### 2.4 The "frame for time T" pattern — the critical signature

```cpp
// Cinematic.h:97
virtual cinData_t ImageForTime(int milliseconds, nvrhi::ICommandList* commandList);
```

Return struct (`Cinematic.h:59-68`):

```cpp
typedef struct {
    int       imageWidth;    // frame width
    int       imageHeight;   // frame height (power of 2)
    idImage*  imageY;        // Y plane  \
    idImage*  imageCr;       // Cr plane  } YUV path (Bink, FFmpeg YUV)
    idImage*  imageCb;       // Cb plane /
    idImage*  image;         // RGBA path (RoQ, or FFmpeg RGB)
    int       status;        // current cinStatus_t
} cinData_t;
```

Contract: the caller passes **absolute wall-clock ms** (typically the
engine's master time plus per-shader offsets). The cinematic computes
`thisTime - startTime`, finds the appropriate decoded frame, and returns
pointers to live `idImage*` textures that are valid *until the next
`ImageForTime` call on this instance*.

The dual return path (YUV planes vs single RGBA) is important: it lets the
shader system do the YUV→RGB conversion on the GPU for Bink/FFmpeg-YUV
streams, saving a CPU colour-space conversion per frame. The RGBA path is
for formats that don't expose YUV planes cheaply.

### 2.5 Lifecycle operations

**Start / allocation** — at material-parse time, from `Material.cpp:1784-1785`:

```cpp
ts->cinematic = idCinematic::Alloc();
ts->cinematic->InitFromFile( token.c_str(), loop, NULL );
```

`NULL` command list is valid here: texture upload is deferred until the
first `ImageForTime`.

**Seek / (re)start** — `ResetTime(int)`:

```cpp
void idCinematicLocal::ResetTime(int time) {
    startTime = time;
    status = FMV_PLAY;
}
```

Note: *every* seek is "set a new origin and rewind the decoder" — there
is no real random seek into the middle of a file; cinematics in id's world
are linear assets played from the start.

**Looping** — handled inside `ImageForTime` (:1340-1349): on end-of-stream,
if `looping`, reset `startTime = thisTime` and flip status back to `PLAY`.
No caller action required.

**Skipping / escape** — there is no built-in skip API. The caller decides:
when the player presses escape, the game sets the cinematic status / calls
`Close()` from its menu layer. The cinematic itself is a pure "frame
dispenser" — it doesn't know about input.

**Rect / positioning** — **none**. The cinematic has **no concept of where
it is drawn.** It returns textures; the shader / material system decides
where those textures go. Fullscreen cinematics, embedded monitors in game
world, HUD overlays — all the same API; the caller binds the returned
`idImage*` to whatever surface it wants.

**Audio** — delegated to a separate `CinematicAudio` abstraction (see
`neo/sound/CinematicAudio.h`), with XAudio2 and OpenAL backends selected
at compile time (:34-36). Interface:

- `InitAudio()` — called during video init
- `PlayAudio(uint8_t* data, int size)` — decoder feeds audio chunks in
- `ResetAudio()` — invalidate buffered audio on seek/loop (Cinematic.cpp:977)

### 2.6 Codec backend dispatch

Compile-time mutual exclusion at `Cinematic.cpp:58-90`:

```cpp
#if defined(USE_FFMPEG)
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #define NUM_LAG_FRAMES 15
#endif

#ifdef USE_BINKDEC
    #ifdef USE_FFMPEG
        #error "Currently, only one of FFMPEG and BINKDEC is supported at a time!"
    #endif
    #include <BinkDecoder.h>
#endif
```

Runtime dispatch inside `ImageForTime` (:1278-1289):

```cpp
if( !isRoQ ) {
#if defined(USE_FFMPEG)
    return ImageForTimeFFMPEG( thisTime, commandList );
#elif defined(USE_BINKDEC)
    return ImageForTimeBinkDec( thisTime, commandList );
#endif
}
// fall through to original RoQ path
```

`isRoQ` is set by `InitFromFile` based on extension. FFmpeg can also
handle `.bik`, but only one of the two backends is compiled in.

### 2.7 Known caller patterns

**Material parse** — `Material.cpp:1784-1785`:

```cpp
ts->cinematic = idCinematic::Alloc();
ts->cinematic->InitFromFile( token.c_str(), loop, NULL );
```

The cinematic is bound to a texture *stage* inside a *material*. One
material can appear on many surfaces; the cinematic is allocated once and
reused.

**Per-frame binding** — `RenderBackend.cpp:268-281`:

```cpp
if( texture->cinematic ) {
    cinData_t cin = texture->cinematic->ImageForTime(
        viewDef->renderView.time[0] +
        idMath::Ftoi( 1000.0f * viewDef->renderView.shaderParms[11] ),
        commandList );
    if( cin.imageY != NULL ) {
        // bind YUV planes and use a YUV-decode shader
    } else if( cin.image != NULL ) {
        // bind single RGBA image
    }
}
```

Time arithmetic: master time + `shaderParms[11] * 1000`, letting the
material author offset the cinematic in time per-surface.

---

## Section 3 — GemRB's BIKPlayer as a plugin

Location: `gemrb/gemrb/plugins/BIKPlayer/`. Much shorter treatment — the
interesting thing about GemRB is the *plugin shape*, not the Bink codec.

### 3.1 Plugin registration

`CMakeLists.txt:6`:

```cmake
ADD_GEMRB_PLUGIN ( BIKPlayer BIKPlayer.cpp dct.cpp fft.cpp
                   GetBitContext.cpp mem.cpp rational.cpp rdft.cpp )
```

`ADD_GEMRB_PLUGIN` compiles the sources into a shared object that GemRB
loads at startup. Resolution is type-based: the engine asks the resource
manager for a `MoviePlayer` via `gamedata->GetResourceHolder<MoviePlayer>()`
(`Interface.cpp:2123`) and the plugin registry dispatches to BIKPlayer
when the resource is a `.bik`. The registration metadata is the `TypeID`
on the base class: `MoviePlayer::ID = { "MoviePlayer" }`
(`core/MoviePlayer.cpp:19`). File-extension mapping is done at the
resource-manager level, not inside the plugin.

### 3.2 Base interface

Inheritance chain:

```cpp
class BIKPlayer : public MoviePlayer { ... };      // BIKPlayer.h:180
class MoviePlayer : public Resource { ... };       // core/MoviePlayer.h:34
class Resource = ImporterBase;                     // core/Resource.h:34
```

`MoviePlayer` is the abstract base. BIKPlayer must implement:

- `bool Import(DataStream*) override` — load/validate the file. Called
  via `Open()` from the resource manager. Returns `true` on valid Bink
  header (`"BIKi"`).
- `bool DecodeFrame(VideoBuffer&) override` — decode **one** frame into
  a caller-provided buffer. Returns `false` on EOF.
- `void Stop() final` — cleanup.

Protected state exposed to subclasses:

```cpp
Video::BufferFormat movieFormat;   // YV12 for Bink
Size                movieSize;
size_t              framePos;
```

### 3.3 BIKPlayer public behaviour

Construction (`BIKPlayer.cpp:66`): set `movieFormat = YV12`.

`Import(DataStream*)` (BIKPlayer.cpp:188-203):
1. Magic check `"BIKi"` (:192)
2. `ReadHeader()` — width, height, fps, audio track count (:193)
3. `sound_init()`, `video_init()` (:198-199)
4. Populate `movieSize.w/h` (:195-196)

`DecodeFrame(VideoBuffer& buf)` (BIKPlayer.cpp:205-236):
1. Check `framePos < header.framecount` (:215)
2. Seek to frame offset in file (:218-219)
3. `DecodeAudioFrame()`, `DecodeVideoFrame()` into `buf` (:223, :227)
4. Pace via `timer_start()` / `timer_wait()` (:231-233)
5. Advance `framePos`; return false on EOF

`Stop()` (BIKPlayer.cpp:238-246): `EndAudio()`, `EndVideo()`, free input
buffer, call `MoviePlayer::Stop()`.

### 3.4 UI-embedded vs fullscreen

**This player is fullscreen-only**, and the playback loop is **blocking**.

- The *caller* (base `MoviePlayer::Play(Window*)`, `core/MoviePlayer.cpp:41-94`)
  creates the render surface — it calls
  `VideoDriver->CreateBuffer(Region(center, size), movieFormat)` (:55), not
  the plugin.
- Positioning is automatic: movies are centred over the caller's window —
  `center = (winFrame.w/2 - size.w/2, winFrame.h/2 - size.h/2)` (:50-52).
- Size is dictated entirely by the video file (`Dimensions()`,
  `MoviePlayer.h:94`).
- `Play()` owns the event loop: it directly calls
  `VideoDriver->PushDrawingBuffer()` and `SwapBuffers()` in a loop (:79,
  :91) and does not return until EOF or stop. There is no "pump one frame"
  API equivalent to OpenMW's `update()` or RBDOOM's `ImageForTime`.
- Colour format: BIKPlayer asks for YV12 (`BIKPlayer.cpp:68`); the
  VideoDriver handles conversion to the display format.

So: *plugin declares what it can fill, engine provides the buffer to fill*.
The plugin never learns about screen position, scaling, or layering.

### 3.5 SDLVideo comparison

`plugins/SDLVideo/` is **not** a movie player. It's a `SDLVideoDriver`, a
sibling of the abstract `Video` driver interface — a low-level rendering
backend (buffer blits, texture creation, swap). BIKPlayer decodes
`.bik` → YV12 frames; SDLVideo draws those frames to the screen.

GemRB's architecture cleanly separates the two:

- **Movie codec** — `MoviePlayer`-derived, answers "what is in this file"
- **Video backend** — `Video`-derived, answers "how do I put pixels on
  the screen"

They're orthogonal plugin hierarchies. A build needs exactly one of each.

---

## Section 4 — Synthesis: the common shape

### 4.1 What all three expose

Every one of the reference implementations has, by some name, these
operations:

| Concept | OpenMW | RBDOOM | GemRB |
|---------|--------|--------|-------|
| Open a file | `playVideo(stream, name)` | `InitFromFile(path, loop, cmdlist)` | `Import(DataStream*)` |
| Query duration | `getDuration()` | `AnimationLength()` | (via header in `Import`) |
| Query dimensions | `getVideoWidth/Height()` | `cin.imageWidth/imageHeight` | `Dimensions()` / `movieSize` |
| Advance playback | `update()` (returns bool) | `ImageForTime(t, ...)` | `DecodeFrame(VideoBuffer&)` (returns bool) |
| Stop / cleanup | `close()` | `Close()` | `Stop()` |
| Is the clip over? | `update()` returns false | `cin.status == FMV_EOF` / `IsPlaying()` | `DecodeFrame()` returns false |
| Per-clip loop | — | `looping` arg to `InitFromFile` | (caller-side re-opens) |

These six concepts — **Open, Duration, Dimensions, Advance/Frame, Stop,
End-detection** — are the "certainly needed" verbs. Every one of the three
exposes them in some form.

### 4.2 What one or two expose (optional)

- **Pause / resume** — OpenMW only (`play/pause/isPaused`). RBDOOM has no
  pause; cinematics are expected to run continuously once started. GemRB
  `Play()` is blocking so pause is not a concept.
- **Seek to arbitrary time** — OpenMW (`seek(double)`). RBDOOM's
  `ResetTime` is effectively "restart at this wall-clock origin" rather
  than "jump to timestamp T in the file." GemRB: no seek.
- **Explicit audio-output injection** — OpenMW's `setAudioFactory` is the
  cleanest: the host passes an audio sink at setup. RBDOOM uses a static
  `CinematicAudio` abstraction with compile-time backend. GemRB: audio
  output comes from the video driver stack, not from a separate hook.
- **Loop flag** — RBDOOM only (at `InitFromFile` time). Others leave it
  to the caller.
- **Per-plane texture output (YUV)** — RBDOOM only. OpenMW `sws_scale`s
  to RGBA inside the decoder. GemRB hands YV12 to the video driver which
  converts.
- **Current-time query** — OpenMW's `getCurrentTime()`; RBDOOM infers it
  (`thisTime - startTime`); GemRB has `framePos` but no caller API.
- **Frame-rate query** — RBDOOM (`GetFrameRate()`) and (implicitly) GemRB
  via header; OpenMW treats this as internal.
- **TGA / frame-dump utility** — RBDOOM only (`ExportToTGA`).

### 4.3 Common patterns

**Frame delivery to the renderer.** The three implementations split
cleanly into two camps:

- **Push-to-texture, renderer just binds** — OpenMW. The renderer holds a
  stable `Texture2D`; the video module replaces its backing `Image` under
  the hood each frame. Zero-copy is possible because `osg::Image::NO_DELETE`
  lets the image reference the FFmpeg RGBA buffer directly.
- **Pull-per-surface, returns live image handles** — RBDOOM. The caller
  asks `ImageForTime(t)`, gets back `idImage*` pointers, and binds them.
  Same instance can be polled multiple times per frame (multiple surfaces
  sharing a material).
- **Caller-provided buffer** — GemRB. Plugin fills a `VideoBuffer` that
  the engine created. This is the inverse of OpenMW's model.

Tradeoff: **push** (OpenMW) is simplest for a single consumer — you wire
up a texture once and forget it. **Pull** (RBDOOM) is better when multiple
consumers want the same cinematic at the same frame — they each call
`ImageForTime` and get consistent results. **Caller-fills-buffer** (GemRB)
decouples codec from render target most sharply, but forces a copy.

**Audio as master clock.** OpenMW and RBDOOM both implicitly or explicitly
make audio the master. OpenMW: `AV_SYNC_AUDIO_MASTER` is the default, with
explicit fallback to external (wall) clock. RBDOOM: the engine's global
time is the clock, but the audio decoder paces itself against real output
consumption, so audio effectively drives the render time. GemRB's
`timer_start/timer_wait` is wall-clock-paced without audio feedback,
which is fine because it owns the whole event loop and doesn't need to
stay in sync with anything external.

**Threading.**

- OpenMW: **three+ internal threads** (parse, video decode, audio decode),
  exposed synchronously through `update()`. Queues with mutexes +
  condvars; atomics for simple flags.
- RBDOOM: essentially **single-threaded from the caller's view** —
  `ImageForTime` decodes on demand (up to `NUM_LAG_FRAMES = 15` ahead for
  FFmpeg). Audio has its own feed because it must. No parse-thread layer.
- GemRB: **blocking, serial** inside `Play()`. No threading needed
  because the call doesn't return until the video is done.

There is no "universal" threading answer. Threading is a function of
*what the host's render loop looks like*, not a property of video playback.

### 4.4 Where the three disagree

These are the design decisions MC2 actually has to make — the references
don't provide a consensus answer.

1. **Does the video system own its pacing or does the host?**
   - OpenMW: host pumps `update()` every render frame; video decides
     whether to upload a new frame.
   - RBDOOM: host supplies a wall-clock time per query; video retrieves
     the matching frame.
   - GemRB: video owns the whole loop; host hands over control until EOF.
   These are three genuinely different integration models and each has
   a matching host-engine shape.

2. **Push vs pull vs fill for frame delivery** (see 4.3) — three
   different answers, directly tied to #1.

3. **Does the video module know about positioning/scaling?**
   - OpenMW: no (it yields a texture; the widget picks geometry).
   - RBDOOM: no (it yields textures; shaders pick geometry).
   - GemRB: sort of — it dictates native dimensions, but the engine
     centres the buffer itself.
   All three converge on "video module does *not* know screen geometry,"
   but they differ on who picks the draw size.

4. **Codec abstraction.**
   - OpenMW: FFmpeg only, not abstracted behind an interface. The codec
     *is* the module.
   - RBDOOM: pluggable FFmpeg/libbinkdec behind a shared `idCinematic`
     interface, selected at compile time, mutually exclusive.
   - GemRB: each codec is a separate *plugin* loaded at runtime via a
     resource-manager lookup.
   Three different points on the "how dynamic is backend selection"
   spectrum.

5. **Audio integration.**
   - OpenMW: explicit `MovieAudioFactory` injected by the host.
   - RBDOOM: compile-time backend in a separate translation unit.
   - GemRB: flows through the video driver stack; no separate audio hook.

6. **Seek.** Three different answers: OpenMW has real seek; RBDOOM has
   "restart at new origin"; GemRB has none.

7. **Pause.** Only OpenMW.

8. **Looping.** RBDOOM bakes it into `InitFromFile`; the others leave it
   to the caller.

9. **Is there a separate audio thread?** OpenMW yes (decoder-owned);
   RBDOOM audio is fed by the video decoder on the render thread;
   GemRB has its own `sound_init` / `DecodeAudioFrame` path driven by
   `DecodeFrame`.

10. **Frame ownership / lifetime guarantees.** OpenMW: texture pointer
    stable for the whole clip, contents change. RBDOOM: `idImage*`
    valid until the *next* `ImageForTime` call. GemRB: caller owns the
    buffer. Three different answers to "when is a decoded frame safe
    to read."

---

*End of reference notes.*
