# Intro sequence: drop CINEMA1.mp4

**Observed (2026-04-25):** Retail MC2 plays only `MSFT.mp4` at startup,
then goes straight to the main menu. `CINEMA1.mp4` is supposed to play
at the **start of the Carver V campaign**, not as part of the engine
intro sequence.

## Current divergence

`code/mainmenu.cpp:165,467,486` hardcodes:

```cpp
std::vector<std::string> introVideos = {"MSFT.mp4", "CINEMA1.mp4"};
```

So the open-source port plays MSFT → CINEMA1 → main menu. Retail plays
MSFT → main menu. CINEMA1 is the Carver V campaign-opening cinematic,
deferred until the player actually starts that campaign.

This regression was probably introduced when the intro sequence was
hand-rebuilt during the FMV port — the dev who wrote it likely saw
CINEMA1 in `full_game/data/movies/`, assumed it was an intro video,
and chained it after MSFT.

## Likely fix

Two-part:

1. **Remove `CINEMA1.mp4` from `introVideos` in `mainmenu.cpp`** at all
   three sites. Intro shrinks to just `{"MSFT.mp4"}`.
2. **Verify CINEMA1 fires from the Carver V campaign-start path.**
   `code/logistics.cpp:407` calls
   `playFullScreenVideo(pVid)` against a campaign-data-supplied video
   name. The campaign data files (search `full_game/data/` for
   `CINEMA1`) most likely already reference it; if so, step 1 is the
   entire fix. If not, the campaign data needs the wiring — out of
   scope for the engine.

## Verification before shipping

- Boot to main menu: only MSFT plays, then menu.
- Start Carver V campaign: CINEMA1 plays as the campaign opener.
- Spot-check CINEMA2-5 still play at their expected campaign points
  (they go through the same `playFullScreenVideo` path so should be
  unaffected, but worth a glance).

## Why it's a followup, not active

Cosmetic / behavioral mismatch, not a crash or correctness bug. The
intro currently shows extra content rather than missing content, so
no one has been blocked. Pick up alongside any other intro/main-menu
work, or as part of FMV cleanup.

## Touch points

- `code/mainmenu.cpp:165` — initial `introVideos` declaration
- `code/mainmenu.cpp:467` — sequence-end check
- `code/mainmenu.cpp:486` — sequence advancement
- `code/logistics.cpp:407` — campaign-driven full-screen video path
  (no change needed if campaign data already references CINEMA1)
- `devlogs/FMV_STATUS.md:132,148,299` — docs say "MSFT → CINEMA1 →
  main menu" as intended behavior. Update those lines when this lands.
