# Release installer (Inno Setup)

Status: functional end-to-end, three polish items left before public
release.

Under `release/` (gitignored). `mc2-mp4-patch.iss` handles wizard,
validation, file copy, transcode, uninstall. Tested end-to-end with
wipe/restore loop.

## Resolved 2026-04-25 — shaders/ payload fix

The payload list was missing `full_game\shaders\*`, so a fresh retail
install booted without GLSL shader sources. `gosRenderMaterial::load()`
opens them with plain `fopen` relative to CWD
(`GameOS/gameos/utils/stream.cpp:78`), bypassing the fastfile fallback
in `mclib/file.cpp`. In release builds `gosASSERT` compiles out, so the
null material is stored and kills the process on the first textured
draw — matching the 7-10 second intro death with `0xC0000005 / RAX=0`
at `mc2.exe+0x1f78b5`. Confirmed by copying `full_game\shaders\` into
`F:\Games\MechCommander2\` and rerunning: game boots clean. Fix is one
`[Files]` line plus an `[UninstallDelete]` entry.

Credit: external evaluator flagged the missing `shaders/` in the
payload and pointed out that the original debug plan (symbolize with
`full_game/mc2.pdb`) was unreliable — the PDB is from 2025-05-09 while
the shipped `mc2.exe` is from 2026-04-23, so the symbol names were not
trustworthy.

Memory lesson: `lesson_packaging_fopen_paths.md`,
`lesson_stale_pdb_trap.md`.

## Remaining before public release

- **Bundle a static ffmpeg.exe** under `release/tools/` (~50 MB
  gyan.dev essentials build) so the installer doesn't require PATH
  ffmpeg.
- **Revert compression to `lzma2/max` + `SolidCompression=yes`** for
  public release. Currently set to `none` for fast iteration builds.
- **Fix Inno deprecation warning**: change
  `ArchitecturesInstallIn64BitMode=x64` to `x64compatible`.

Not blocking for development, but required before shipping.

## Open question — distribution model

ThranduilsRing is building a mod-launcher + Exodus-as-mod distribution
model. Either retire our Inno installer once his launcher is canonical,
or keep it as a "vanilla-plus-fixes, no mods" entry point. Awaiting his
response on discussion #2.
