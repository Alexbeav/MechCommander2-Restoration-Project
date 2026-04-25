# Release Packaging — 2026-04-24 Session

> **FMV crash resolved 2026-04-25.** The CWD-dependent crash described below
> was caused by the installer payload missing the top-level `shaders/`
> directory. See `release_packaging_2026-04-25_shaders_fix.md`.

**Branch:** `fmv-mp4-support`
**Outcome:** Installer built and tested end-to-end. One new FMV-playback crash surfaced
that is **CWD-dependent, not installer-dependent** — kicked to follow-up.

## What was built

Installer lives under `release/` (gitignored). Inno Setup 6 script, staged binaries
and fork data, with a license-anchor validator on the Select Directory page.

```
release/
├── README.txt
├── bin/                  15 files, 158 MB — mc2.exe, mc2res_64.dll,
│                         SDL2, GLEW, zlib, FFmpeg DLLs
├── assets/graphics/      24 files — font .bmp/.glyph overlay
├── tools/
│   ├── transcode-bik-to-mp4.ps1   (install-time)
│   └── restore-bik.ps1            (uninstall-time)
└── installer/
    ├── mc2-mp4-patch.iss          (Inno Setup script)
    ├── build-installer.bat        (runs iscc.exe)
    └── output/                    (compiled setup.exe lands here)
```

Compression is currently `none` / `SolidCompression=no` for fast iteration. Revert
to `lzma2/max` + `SolidCompression=yes` for public release.

## Wizard flow (what the .iss does)

1. Welcome page — declares retail MC2 prerequisite.
2. Select Directory — auto-detects via `HKLM32\SOFTWARE\Microsoft\Microsoft Games\MechCommander 2\1.0\INSTALL_PATH`,
   falls back to HKLM64, then to common hand-install paths.
3. `NextButtonClick(wpSelectDir)` validator:
   - **Hard check**: `Mc2Rel.exe` must exist. Blocks Next with error MsgBox. License anchor.
   - **Soft check**: `Mc2Rel.exe` size should be 3.5–6 MB (retail is 4.38 MB). Warns, user can override.
   - **Hard check**: `data\movies\` must exist. Blocks Next.
4. File copy — `bin/`, `assets/`, `tools/`, plus fork data from `full_game/` bundled
   verbatim (private-test payload, see Pending follow-ups in `claude.md`).
5. Post-install run — `transcode-bik-to-mp4.ps1` walks `data\movies\*.bik`, transcodes
   via bundled or PATH ffmpeg, idempotent. Moves source `.bik` to `bik_backup/`.
6. Start Menu / optional desktop shortcut pointing at `mc2.exe`.
7. Uninstaller — auto-generated. Pre-runs `restore-bik.ps1` to move `.bik` back and
   delete `.mp4` twins, then removes our binaries. Mc2Rel.exe is never touched.

## What made the install actually boot

Iterated against `F:\Games\MechCommander2\` (wipe + restore from
`F:\Games\MechCommander2 - Clean\` between runs). Three real data gaps surfaced in
sequence, each fixed in the installer payload:

1. **Missing font `.bmp`/`.glyph` files** — retail ships `.d3f`/`.tga`, the fork's
   build hardcodes `.bmp`/`.glyph` at `GameOS/gameos/gameos_graphics.cpp:2190-2191`.
   Fixed: overlay `full_game/assets/graphics/*.{bmp,glyph}` into install.
2. **Missing `data/campaign/campaign.fit`** — not in retail at all, not in any retail
   `.fst` archive (confirmed by string-searching every retail .fst). The fork's data
   is produced from the separate `mc2srcdata` repo per `BUILD-WIN.md`. Fixed: bundle
   the full fork `data/` tree in the installer.
3. **Incompatible `options.cfg`** — retail has `l Resolution = 0` (single combined
   field). Fork reads `l ResolutionX` and `l ResolutionY` separately. Mismatch caused
   an access violation during settings load / GL context setup. Fixed: overwrite
   retail's `options.cfg` with the fork's version.

After these three fixes the game boots past font load, campaign load, all `.fst`
archive mounts, and all `Large allocation` warnings. Log matches
`full_game/mc2.exe` baseline 1:1 up to and including `[MP4Player] First frame
decoded: PTS=0 texReady=1 texID=44 1920x1080`.

## Remaining issue: CWD-dependent FMV crash

After all the above, mc2.exe crashes mid-MSFT-intro with
`0xC0000005 (access violation)` consistently at `mc2.exe+0x1f78b5`, around
7–10 seconds into the 13-second MSFT.mp4 playback.

The crash is **tied to the install directory, not to the binary or the data**:

- **Same binary, different CWD, different outcome.** Running
  `F:\games source code\games\mc2\full_game\mc2.exe` (known-good) with
  `WorkingDirectory = F:\Games\MechCommander2` reproduces the exact same crash
  (`0xC0000005`, ~8.4s in, same fault offset).
- **Files are functionally identical.** `mc2.exe`, `mc2res_64.dll`, `MSFT.mp4` all
  md5-match between locations. Every `.fst` archive size-matches. `options.cfg` and
  `system.cfg` SHA256-match.
- **Not DLL collisions.** Moving retail's `mc2res.dll` and `binkw32.dll` out of the
  install dir (to `_quarantine/`) does not change the crash.
- **Not loose-dir missing content.** Copying `full_game/{art,effects,missions,multiplayer,objects}/`
  into the install dir does not change the crash.
- **Not Defender first-scan.** Consecutive runs of `mc2.exe` against the install dir
  crash identically (7.2s, 7.2s), ruling out realtime-scanning latency.

**Evidence artifacts on disk:**
- Crash dumps: `C:\Users\Alexbeav\AppData\Local\CrashDumps\mc2.exe.*.dmp` (5 dumps,
  ~35 MB each).
- Symbols: `F:\games source code\games\mc2\full_game\mc2.pdb`.
- Event log: `Application` log shows `Application Error` entries with
  `Exception code: 0xc0000005 / Fault offset: 0x00000000001f78b5 /
  Faulting application path: F:\Games\MechCommander2\mc2.exe`.

The log never gets past `[MP4Player] First frame decoded` — no STOP, no Assert,
no `ERROR:` line. Audio stutters (user-observed), video texture is created but not
drawn before the crash.

Investigation should start by symbolizing `mc2.exe+0x1f78b5` via WinDbg or
`dumpbin /disasm` with the pdb, then follow the call path from there. See
`RESUME_2026-04-25.md` for the resume prompt.

## Other notes

- Inno's `[Run]` entry with `postinstall + nowait + skipifsilent` flags still
  launched mc2.exe during `/VERYSILENT` installs and orphaned it when Inno exited
  (process terminated by Windows when parent exited). Fixed by launching
  mc2.exe from outside Inno for testing. For the public release, leaving the
  `postinstall` Run entry is fine under interactive install; it only orphans under
  silent install, which is a developer path.
- The transcoder PS1 is idempotent and safe to re-run. Verified: installing over
  a previously-installed state skips already-transcoded `.mp4` files and leaves
  the `bik_backup/` state intact.
- Inno deprecation warning `Architecture identifier "x64" is deprecated` — should
  switch `ArchitecturesInstallIn64BitMode=x64` to `x64compatible` for Inno 6.3+.
  Not blocking.
