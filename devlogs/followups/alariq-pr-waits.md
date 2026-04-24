# Upstream PRs awaiting alariq

Per-PR outreach history and status. Short table lives in
`devlogs/project_state.md`.

## Open PRs

### [#23 — UI scaling](https://github.com/alariq/mc2/pull/23)

- Opened 2025-04-04 from our fork.
- Three rounds of review; our last update 2025-09-11
  ("Implemented your comments @alariq"); no response since.
- Rebased onto current `upstream/master` on 2026-04-24 to recompute
  mergeability and surface in alariq's inbox as "updated."
- Next: light ping comment anchoring off the existing conversation.

### [#24 — Windows build improvements](https://github.com/alariq/mc2/pull/24)

- Opened 2025-07-15.
- Only copilot review so far; no alariq activity.
- Rebased on top of updated #23 on 2026-04-24; blocked on #23 merging
  first.
- Don't ping cold — no prior conversation to anchor, reads pushy.

### [#26 — FMV pipeline](https://github.com/alariq/mc2/pull/26)

- Opened 2026-04-24. Branch `fmv-ffmpeg-pipeline`. Closes #22 (FMVs
  missing, user-opened).

### [#28 — Campaign-dialog dup fix](https://github.com/alariq/mc2/pull/28)

- Opened 2026-04-24. Closes #27. Trivial review, easiest possible yes.

### [#29 — Compass alpha-test fix](https://github.com/alariq/mc2/pull/29)

- Opened 2026-04-25. One-line fix in `mclib/txmmgr.cpp`.

## Maintainer activity

alariq pushed 4 commits to master on 2026-04-23 (`8dd96f8`, `414cf38`,
`9740e68`, `53c5484`) but did not act on any of our PRs. None of his
changes overlap with ours.

## Cadence

Rebase + force-push when we have something new to ship, to surface
the PRs in his "updated" feed. Light ping comments only on threads
where we already have an existing conversation to anchor on. Don't
stack pings.
