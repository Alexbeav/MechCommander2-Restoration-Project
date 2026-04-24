# Archived local branches

Log of purely-local branches that were archived and deleted. Each
entry records the tip SHA, date, and archive location, so the objects
can be restored from the bundle if ever needed.

## Why this file exists

Some branches live only on a specific developer's machine (never
pushed to `origin`), accumulate WIP checkpoints during exploratory
work, then get superseded when the polished result lands on `master`
via squash-merge. Keeping them as live branches adds ambiguity
without preserving anything load-bearing; deleting them without
any archive forgoes the escape hatch if something unexpected turns
out to be needed later.

Policy: **bundle + delete, not push to origin.** Pushing `archive/*`
branches pollutes the remote branch list and invites accidental
checkout by other contributors. A local bundle file preserves the
git objects cleanly and stays out of everyone else's way.

Restore from a bundle:

```
git fetch <bundle-path> <branch-name>
```

## 2026-04-25 archival

Two local branches archived and deleted. All commits on both were
authored by Alexandros Mandravillis, never pushed to any remote.
Verified beforehand that no unique files, third-party contributions,
or master-behavioral differences were at risk — see the evaluator
analysis in the conversation history.

| Branch | Tip SHA | Tip date | Tip message |
|---|---|---|---|
| `dev` | `fc8620c86e9657317c15720d76198e8be5bc2a9b` | 2025-07-15 | merge remaining files |
| `fmv-mp4-support-backup` | `5234baf918f1326f3ccd4e07bd0c7057f482f128` | 2025-07-20 | FMV works in-game, frame rate is wrong & no audio. game boots to main menu |

**Archive location:**
`F:\games source code\games\mc2-archive-local-branches-2026-04-25.bundle`
(sibling of repo root; 29,811,934 bytes; sha1; complete history).

Verified with `git bundle verify` before branch deletion. Restore
either branch with:

```
git fetch "F:\games source code\games\mc2-archive-local-branches-2026-04-25.bundle" dev
git fetch "F:\games source code\games\mc2-archive-local-branches-2026-04-25.bundle" fmv-mp4-support-backup
```

Evaluator notes on the decision:
- No unique behavior that alters the current `master` build.
- No unique files except CMake build detritus, a WIP scratch
  subdirectory (`code/backup - not working, not crashing/`), and a
  stray `patch.patch`.
- Commit messages were WIP checkpoints
  (`[TEST] new improvements`, `before switching model`, `before grok
  changes`, `current testing, still gibberish`), not finished work.
- FMV pipeline has been stable on master for 2+ days post-archive,
  with all surfaces verified (intro, cinematics, briefing, pilot-
  portrait videos).
