# Authoring Custom Campaigns for MechCommander 2

This guide explains how to author a custom MechCommander 2 campaign without
modifying the engine or building new assets. It's written to be followed by
either a modder directly or an AI coding agent (Claude Code, Cursor, etc.)
working through the repo on their behalf — steps are concrete, paths are
explicit, and templates are copy-paste-ready.

If you want to build *new* maps, units, or videos, you need the mission
editor (see `devlogs/mission_editor_tier0_plan.md`). If you want to
**remix existing assets into a new campaign arrangement with custom
pacing, custom video sequencing, and a custom name**, this guide is
sufficient — no code changes required.

## How the engine discovers campaigns

- At launch → **New Campaign**, the campaign-select list is built by
  `LogisticsSaveDialog::initDialog()` in `code/logisticsdialog.cpp`:
    1. `FindFirstFile` scans `data/campaign/*.fit` and adds every match.
    2. A defensive block then adds the hardcoded names `campaign` and
       `tutorial` if those `.fit` files exist (covers retail installs
       where campaigns are packed inside fastfiles).
- As long as your new `.fit` file lives in `data/campaign/`, the scan
  picks it up. No registration, no enum, no recompile.
- The display name comes from `LogisticsSaveDialog::readCampaignNameFromFile()`
  in `code/logisticsdialog.cpp`. It prefers `NameID` (resource-DLL string
  lookup) and falls back to the string literal `CampaignName`. Use
  `CampaignName` unless you're also shipping a patched `mc2res_64.dll`.

## What a campaign is made of

A campaign is one `.fit` file that points at other existing files. Every
pointer is a basename that the engine combines with a known path prefix
and extension.

| Field | Resolves to | What it is |
|---|---|---|
| `FileName` on a Mission block | `data/missions/<name>.{fit,pak,abl}` | Mission map + objectives + AI script |
| `OperationFile` on a Group block | `data/art/<name>.fit` (+`.tga`) | Operation-map screen between missions |
| `PurchaseFile` on a Mission block | `data/missions/<name>.fit` | Starting mech/weapon loadout |
| `PreVideo` on a Group block | `data/movies/<name>.mp4` | Full-screen briefing before that group starts |
| `Video` on a Group block | `data/movies/<name>.mp4` | Small thumbnail video on the mission-select screen |
| `FinalVideo` on the Campaign block | `data/movies/<name>.mp4` | Played after the campaign is complete |
| `ABLScript` on a Group block | ABL script name (no extension) | Group-level AI/event script |
| `Tune` on a Group block | Integer music-track ID | Background music |

Extensions like `.bik` in video field values are stripped automatically
(`Logistics::playFullScreenVideo()` in `code/logistics.cpp`). You can
use bare basenames and they'll work.

## Step-by-step: author a 3-mission test campaign

### 1. Decide your scope

For a proof-of-concept, reuse retail assets. Every reference below points
at a file that ships with retail, so the campaign runs without any new
content. Pick three of the 25 retail missions at `data/missions/mc2_01`
through `mc2_25` and three existing videos from `data/movies/` (e.g. the
`CHOICE_*` or `CINEMA*` sets).

### 2. Write the `.fit`

Create `data/campaign/<your_name>.fit`. Minimal 3-mission template:

```ini
FITini


[Campaign]

st CampaignName = "Your Custom Campaign"
l GroupCount = 3
l CBills = 80000
st FinalVideo = "CINEMA5"

[Group0]

l NumberToComplete = 1
l MissionCount = 1
st OperationFile = "MCL_CM_op1_1"
st Video = "STANDIN"
st PreVideo = "CHOICE_1"
l Tune = 22
st ABLScript = "Tutorial0"

[Group0Mission0]

st FileName = "mc2_01"
b Mandatory = TRUE
st PurchaseFile = "Purchase01"
b PlayLogistics = 0
b PlaySalvage = 0
b PlayPilotPromotion = 1
b PlayPurchasing = 0
b PlaySelection = 0

[Group1]

l NumberToComplete = 1
l MissionCount = 1
st OperationFile = "MCL_CM_op1_2"
st Video = "STANDIN"
st PreVideo = "CHOICE_2"
l Tune = 22
st ABLScript = "Tutorial1"

[Group1Mission0]

st FileName = "mc2_02"
b Mandatory = TRUE
st PurchaseFile = "Purchase02"
b PlayLogistics = 1
b PlaySalvage = 1
b PlayPilotPromotion = 1
b PlayPurchasing = 1

[Group2]

l NumberToComplete = 1
l MissionCount = 1
st OperationFile = "MCL_CM_op1_3"
st Video = "STANDIN"
st PreVideo = "CHOICE_3"
l Tune = 22
st ABLScript = "Tutorial2"

[Group2Mission0]

st FileName = "mc2_03"
b Mandatory = TRUE
st PurchaseFile = "Purchase03"
b PlayLogistics = 1
b PlaySalvage = 1
b PlayPilotPromotion = 1
b PlayPurchasing = 1
```

### 3. Drop it into the install

Copy `<your_name>.fit` into `<mc2-install-dir>/data/campaign/`. Do **not**
overwrite `campaign.fit` or `tutorial.fit`.

### 4. Launch and verify

1. Run `mc2.exe`.
2. **New Campaign** → the list should show three entries: *Carver V*,
   *Tutorial*, and the one named in your `CampaignName`.
3. Pick yours → enter a save name → play through.
4. Each group's `PreVideo` should play full-screen before its mission
   starts. `FinalVideo` plays after the last mission.

If anything doesn't play or appear, check `<mc2-install-dir>/mc2_stdout.log`
for `[MP4Player]` / `[MC2Movie]` lines that show which files the engine
tried to open.

## `.fit` format notes

### Line syntax

```ini
<type-prefix> <Key> = <Value>
```

Type prefixes:
- `l` — long (integer)
- `st` — string (quoted if it has whitespace)
- `b` — boolean (`TRUE` / `FALSE` / `0` / `1`)
- `f` — float

The parser is permissive about whitespace around `=` and is
case-insensitive on keys. Comments start with `//`.

### Block structure

- `[Campaign]` — one, at the top. Holds campaign-wide fields.
- `[GroupN]` — one per narrative stage, zero-indexed contiguous (`Group0`,
  `Group1`, …). Count must match `GroupCount` on the campaign block.
- `[GroupNMissionM]` — one per mission within a group, zero-indexed
  contiguous. Count per group must match that group's `MissionCount`.

### Mission-level flags (Play*)

These control which between-mission screens show after completing the
mission. Typically `0` on the first mission (no inventory yet) and `1`
on subsequent ones:

- `PlayLogistics` — show the logistics/dropship screen.
- `PlaySalvage` — show the salvage-pick screen.
- `PlayPilotPromotion` — show pilot-promotion dialogs.
- `PlayPurchasing` — show the purchasing screen.
- `PlaySelection` — show the mission-selection screen between this
  mission and the next (used in multi-mission groups).

### Per-mission overrides

- `VideoOverride` on a `[GroupNMissionM]` block replaces the group's
  `Video` field for that specific mission's thumbnail.

## Gotchas

- **The list shows only two entries and my campaign isn't there.** The
  pre-2026-04 dup-bug was that retail campaign and tutorial showed twice
  and your custom one was pushed off the visible list. If you're on an
  older build, update. Master `post-2026-04-24` has the scan-dedup fix.
- **Videos don't play at all in the briefings.** Two bugs existed
  pre-2026-04-24: `.bik` extensions were appended to `.mp4` producing
  `cinema1.bik.mp4`, and the big-video player was deleted on frame 1 due
  to a missing `isPlaying()` check. Both fixed in `code/logistics.cpp`.
- **`FileName = "mc2_01"` but no mission loads.** Check that
  `data/missions/mc2_01.pak` exists. The `.fit`/`.abl`/`.pak` triplet
  must all be present.
- **`PurchaseFile` referenced doesn't exist.** Engine asserts. Point at
  an existing one like `Purchase01` or write your own.

## Using an AI coding agent

This guide is structured so an agent can follow it as a procedure:

1. **Prompt template:**

   > Author a custom MechCommander 2 campaign at
   > `data/campaign/my_campaign.fit` with `GROUP_COUNT` groups, each
   > containing one mission. Use the template in `CUSTOM-CAMPAIGNS.md`.
   > Reuse retail mission files `mc2_01` … `mc2_NN` for the `FileName`
   > fields, retail operation files `MCL_CM_op1_1` … for `OperationFile`,
   > retail purchase files `Purchase01` … for `PurchaseFile`. Use
   > `CHOICE_1`, `CHOICE_2`, `CHOICE_3` as `PreVideo` entries and
   > `CINEMA5` as `FinalVideo`. Display name: `"DISPLAY_NAME"`.

2. **What the agent should verify before calling the task complete:**
   - All `FileName`, `OperationFile`, `PurchaseFile`, `PreVideo`, and
     `FinalVideo` values correspond to files that already exist under
     `data/missions/`, `data/art/`, or `data/movies/`. Use the Glob or
     directory listing tools to confirm.
   - `GroupCount` matches the number of `[GroupN]` blocks.
   - Each group's `MissionCount` matches the number of
     `[GroupNMissionM]` blocks within it.
   - Groups and missions are contiguous and zero-indexed.

3. **What the agent can't do on its own:** authoring the mission `.pak`
   contents, operation-map art, new videos, or new pilot data. Those
   need the mission editor (see the plan under `devlogs/`).

## Worked example

The Restoration Project ships a 3-mission test campaign authored by this
exact process. See `full_game/data/campaign/custom_test.fit` in the repo
(or the release install). It uses `CHOICE_1/2/3` as briefings,
`mc2_01/02/03` as missions, and `CINEMA5` as the finale. Total content
authored: one `.fit` file, zero new binary assets.
