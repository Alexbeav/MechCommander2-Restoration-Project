# Distribution strategy and license anchor

Discovered 2026-04-24 while building the MP4 FMV release bundle.

alariq's fork is **not** a patch over retail — it's a full replacement.
Retail `.fst` archives differ in both size and content from the fork's;
retail has no `data/campaign/campaign.fit` at all (confirmed by
string-searching every retail `.fst`); the fork's data is produced
from the separate `mc2srcdata` repo per `BUILD-WIN.md` and is not
derivable from retail files.

## Legal constraint

We do **not** want to publish a GitHub distribution that includes the
game's data/assets, even though MC2 is abandonware. Pattern to follow:
Falcon BMS's "bring your own Falcon 4" approach — distribution
includes patched binary + our data, but requires the user to supply a
retail `Mc2Rel.exe` as a license anchor.

## Current state

- `release/Install-Patch.bat` enforces the anchor at install time by
  refusing to run unless `Mc2Rel.exe` is present in the target
  directory.
- `mc2.exe` itself does **not** check for `Mc2Rel.exe` at startup. An
  attacker can bypass the installer's check by creating an empty file
  of that name.

## Open — runtime anchor check in `mc2.exe`

Add an early-init check (likely in `GameOS/gameosmain.cpp` before SDL
init) that calls `fileExists("Mc2Rel.exe")` and aborts with a
MessageBox if missing. ~15-20 lines. Makes the anchor harder to strip
without actually patching the binary. Not DRM — just a formal
declaration of the ownership requirement.

## Open — lean distribution pass

Initial v1 bundle ships the fork's `data/` tree verbatim (~1.9 GB).
Most of that is loose unpacked files that duplicate content already
packed inside `.fst` archives. `mclib/file.cpp:246-254` (`File::open`)
tries loose first, then falls back to `FastFileFind()` in registered
fastfiles. So if we strip the loose duplicates and ship only the
`.fst` archives plus files that aren't packed (at minimum:
`data/campaign/campaign.fit`), the binary should boot identically from
`.fst`. Expected size: 400-600 MB instead of 2 GB.

Method: iterative boot-retry. Ship minimum (`.fst`s + obvious
non-packed like `campaign/`), read `mc2_stdout.log` after each failed
launch, add back only what it couldn't find, repeat until clean boot.
3-5 cycles probably. Brittle against future fork changes but good for
one-shot v1 size reduction.

Leaner distribution also reinforces the license-anchor story: smaller
the data blob, the more the value-add lives in the binary the user
already owns (`Mc2Rel.exe` as evidence of entitlement to the asset set
our patch extends).
