# Font tweaks — residual after D3F loader work

Status: open. Spawned from
[`devlogs/closed/fonts_d3f_loader_2026-04-25.md`](../closed/fonts_d3f_loader_2026-04-25.md)
when the menu-fonts portion landed clean (commits `0a1ed6b`,
`3a0bb12`, `a9e7b46` on the `fonts` branch).

The .d3f loader, calibration, sidecar bridge, visual-bounds API, and
the three centering rewires are all in production. The five
verification screens (Main Menu, Mission Selection, Mission Briefing,
Mech Bay, Pilot Ready, in-mission HUD button) look correct. What's
left is scattered, screen-by-screen polish on text that doesn't go
through the rewired centering paths.

## Active items

### Top-aligned screen headers sit too high

`MECH BAY`, `MISSION SELECTION`, `MISSION BRIEFING`, `PILOT READY AREA`,
`LOAD GAME` (and likely `MAIN MENU` proper, the salvage screen, etc.) —
the top-left corner labels of each major screen — render via a
top-aligned path, not through `aButton::render` /
`ControlButton::render` / `aTextListItem::render` (the three sites
rewired to `gos_TextVisualBounds()`). They sit a few px higher in
their text region than retail.

Need to identify the render path. Likely candidates:
- `aText` widget (`gui/atext.cpp`) used directly with explicit
  position
- A `.fit`-driven text element with `YLocation` baked in
- One of the `drawShadowText` paths

Once identified, either: (a) shift the y-coordinate by the visible
top-trim amount (the `top_trim` from `calibrate_vertical`), or
(b) wire the path through the visual-bounds API if it does any
centering.

### Other in-game fonts elsewhere

User flagged generally that "there's still some tweaking to do on
other fonts elsewhere" without specifics. Likely candidates worth
inspecting:
- Salvage screen text
- Encyclopedia entries
- Multiplayer lobby
- In-mission floating text (waypoint labels, building names) — these
  go through the `globalFloatHelp` system; bounds calls there
  currently use `gos_TextStringLength()` for background-rect sizing,
  which is acceptable (slight overhang is invisible) but could be
  switched to visual-bounds for tighter fit.
- Pilot review / promotion screens
- Save/load dialog details

Not a single fix — each screen needs an eyeball pass against retail
references before deciding whether anything's actually wrong.

## Deferred (carried forward, not fixable in current architecture)

### Stroke-weight upscale artifact

D3F atlases are pure 1-bit (alpha is 0 or 255, no antialiasing). At
2×+ display upscale with nearest filtering, every 1-px stroke becomes
2-3 display pixels, reading as a heavier weight than retail's
near-1:1 render. Confirmed in the closed devlog by inspecting
`arialnarrow8.d3f` (header `weight=400`, regular) against the legacy
`.bmp` atlas (also 1-bit, same pixel density).

Not a loader bug. Real fix is one of:
- Linear filtering for fonts (tradeoff: blurry at non-integer scales)
- SDF font system (deep rework)
- Higher-resolution source atlases (no source pipeline exists)

Defer indefinitely unless someone seriously commits to a font system
overhaul.

### Mech-slot label width spillage

Legacy `.glyph` for `arialnarrow8` has zero-width entries for
lowercase letters (the legacy `.bmp` atlas only stores uppercase +
digits). Retail-via-D3F shows full lowercase, so labels like
"BUSHWACKER\nLongshot" now render with a wider visible footprint
than the legacy port did.

Correct rendering behavior. Real fix is widening the slot box in
`full_game/data/art/mcl_gn_deploymentteams.fit` (the
`[MechNameText]` and `[PilotNameText]` blocks at lines 22-34). UI
layout work, not loader work. Touch when somebody's already in the
mech-bay UI.

## Out of scope here

- `TF_R8` font-local optimization (VRAM halving for atlases) —
  already noted in the closed devlog as low priority.
- Multi-atlas v4 fonts (`iTextureCount > 1`) — not observed in the
  local corpus; loader rejects with SPEW.
- Asset cleanup of redundant `.bmp`+`.glyph` files in
  `full_game/assets/graphics/` — asset-cleanup commit, not code work.
  The `.glyph` sidecars are still needed for the `font_line_skip_` /
  `max_advance_` bridge.
