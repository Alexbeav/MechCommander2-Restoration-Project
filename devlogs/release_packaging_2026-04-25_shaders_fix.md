# Release Packaging — 2026-04-25: Shaders Fix

**Branch:** `fmv-mp4-support`
**Outcome:** The CWD-dependent FMV crash blocking the 2026-04-24 installer release
is **resolved**. Root cause: the installer payload did not ship the top-level
`shaders/` directory. Adding it makes fresh retail installs boot cleanly.

## Root cause

`gosRenderMaterial::load()` opens GLSL source relative to CWD:

- `GameOS/gameos/gameos_graphics.cpp:224` builds paths as `shaders/%s.vert` and
  `shaders/%s.frag`.
- `GameOS/gameos/utils/shader_builder.cpp:138` hands the paths to
  `GameOS/gameos/utils/stream.cpp:78`, which calls plain `fopen`. This bypasses
  the loose-file-then-fastfile fallback in `mclib/file.cpp:245`.

When the files don't exist, the open fails, `gosRenderMaterial::load()` returns
`NULL`, and in release builds `gosASSERT` compiles out
(`GameOS/include/gameos.hpp:102`). The null material is stored into
`gos_render_materials[]` and the process continues. The first textured quad
draw then dereferences the null — the crash we saw at `mc2.exe+0x1f78b5`
(`0xC0000005` with `RAX = 0`).

This matches every symptom we had, including why the crash was **CWD-dependent,
not binary-dependent**:

- `full_game/mc2.exe` and the installed copy are byte-identical (MD5
  `6C45D3C0…`), so the crash offset is the same wherever you run it.
- `full_game/` contains `shaders/` at the same level as `mc2.exe`, so running
  from that CWD loads shaders fine.
- The fresh retail install did not contain `shaders/`, so running from
  `F:\Games\MechCommander2` found nothing and stored `NULL` materials.
- Timing (7–10 seconds into the 13-second MSFT intro clip) is when the first
  textured quad in the main-menu / mission-selection splash renders.

## How we got here

External evaluator spotted two things the 2026-04-24 session missed:

1. The `[Files]` section in `release/installer/mc2-mp4-patch.iss` had no entry
   for `full_game\shaders\*`. Every other directory the binary needs was
   enumerated (`bin`, `assets`, `tools`, `.fst` archives, `data/*`, `1033/`,
   and selective `full_game/assets/*`) — `shaders/` was simply overlooked.
2. The original debug plan ("symbolize `mc2.exe+0x1f78b5` using
   `full_game/mc2.pdb`") was unreliable. The PDB on disk is dated 2025-05-09
   and the shipped `mc2.exe` is dated 2026-04-23. Symbol names from that PDB
   would not correspond to offsets in the current binary, so WinDbg's
   `!analyze -v` output would have been misleading even if we'd run it.

Verification took one step: `robocopy full_game\shaders
F:\Games\MechCommander2\shaders /E`, then launch. Game booted clean, MSFT
intro played through, menus rendered correctly.

## Installer fix

`release/installer/mc2-mp4-patch.iss`:

- Added to `[Files]` (right after the `bin\*` copy, since shaders are required
  for the binary to render — not optional data):

  ```inno
  Source: "..\..\full_game\shaders\*"; DestDir: "{app}\shaders"; \
      Flags: ignoreversion recursesubdirs createallsubdirs
  ```

- Added to `[UninstallDelete]`:

  ```inno
  Type: filesandordirs; Name: "{app}\shaders"
  ```

`full_game/shaders/` contains 12 files (6 `.vert` / `.frag` pairs) plus
`include/` with 2 `.hglsl` headers. Total ~20 KB — negligible payload bump.

## Lessons

- **Check the install manifest against what the binary `fopen`s.** The
  fastfile system hides path dependencies for archive-resident data, but any
  file the binary opens with plain `fopen` relative to CWD is an implicit
  requirement that has to be enumerated in the packager. Shaders fell in that
  bucket and slipped through.
- **A stale PDB is worse than no PDB.** If you're going to rely on symbol
  names, verify the PDB matches the binary (build timestamps, mod-time, or
  `dumpbin /headers`). Had we checked first, we'd have known the WinDbg path
  wouldn't give trustworthy names and looked elsewhere earlier.
- **Null dereferences with `RAX = 0` deserve their own checklist.** In a
  renderer that stores heterogeneous objects in a fixed-size array indexed by
  handle, a `NULL` entry from a failed load is a classic pattern worth
  screening for before diving into live-debugging.

## What's still open

Installer polish items tracked under "Release installer" in `claude.md`:

- Bundle a static `ffmpeg.exe` under `release/tools/`.
- Revert `Compression=none` back to `lzma2/max` + `SolidCompression=yes` for
  public release builds.
- Rename `ArchitecturesInstallIn64BitMode=x64` to `x64compatible` to silence
  the Inno deprecation warning.

None block daily development; all are required before the public release ZIP
ships.
