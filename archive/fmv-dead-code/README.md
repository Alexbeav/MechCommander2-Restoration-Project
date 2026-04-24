# Archived FMV Work

These files are **not part of the current build** and are kept here for historical reference
only. They represent abandoned approaches from earlier iterations on the FMV playback task.

See `../../FMV_STATUS.md` for the current design and why each of these was dropped.

## Contents

### `code/` — abandoned C++ files

| File | Was part of | Reason archived |
|---|---|---|
| `mc2movie.cpp` / `.h` | Earlier OpenGL rendering wrapper around `MP4Player` | Rendering now lives directly in `MP4Player`. |
| `standalone_mp4_player.cpp` | Old game-engine-dependent test harness | Replaced by `code/clean_mp4_player.cpp`. |
| `video_position_fix.cpp` / `.h` | Band-aid positioning helper | Band-aid on a broken pipeline; correct fix is logical→physical scaling inside `MP4Player::render()`. |
| `frame_rate_fix.cpp` / `.h` | Band-aid frame-pacing helper | Superseded by PTS-gated display + audio-master clock. |
| `fmv_test_framework.cpp`, `fmv_test_runner.cpp` | CTest-integrated FMV test framework | Never produced actionable signal; standalone player is a simpler validator. |
| `mp4player_test_extension.cpp` / `.h`, `test_metrics.h`, `video_validator.cpp` | Helpers for the above test framework | Dead with the framework. |

### `scripts/` — abandoned test scripts / scratch output

| File | Purpose | Reason archived |
|---|---|---|
| `test_fmv.ps1`, `quick_fmv_test.ps1`, `test_videos.ps1`, `mp4_standalone_test.ps1` | PowerShell drivers for the FMV test framework | Dead with the framework. |
| `CMakeListsTestAddition.txt` | Snippet appended to `CMakeLists.txt` to add the test targets | The targets are gone. |
| `debug_output.txt`, `fmv_quick_test.txt` | One-off log captures | Just scratch. |

### `docs/` — superseded documentation

| File | Superseded by |
|---|---|
| `VIDEO_FIXES_SUMMARY.md` | `../../FMV_STATUS.md` |
| `FMV_TESTING_GUIDE.md` | `../../FMV_STATUS.md` |

These docs describe an architecture that no longer exists (MC2Movie-centric, with
`video_position_fix` / `frame_rate_fix` helpers). Do not follow their instructions.

## If you want to restore any of this

These are kept so the context isn't lost, not because they should be resurrected. Before
restoring anything, read `FMV_STATUS.md` to understand why it was dropped in the first place.
