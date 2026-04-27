# Input — 1-pixel-column escape on multi-monitor

Spawned 2026-04-27 when the 4-commit input-fixes branch landed on
master (commits `16fdd7a`, `729f9a8`, `803a64f`, `0f8dcaf`).

## What works now (on master)

The full mouse-grab stack:

- `gos_SetMousePosition` normalized→pixel conversion is correct.
- `SDL_SetWindowMouseGrab` engaged at window creation.
- The dead `process_events()` window-event subevent dispatch is fixed
  (top-level cases for `SDL_WINDOWEVENT_RESIZED` / `FOCUS_LOST` were
  matching `event.type` when those codes live in `event.window.event`
  — they could never fire). Resize handling, focus tracking, and
  grab-reapply on focus all now actually work.
- Native Win32 `ClipCursor` reinforcement on top of the SDL grab,
  driven by `FOCUS_GAINED` / `FOCUS_LOST` / `MOVED` / `RESIZED`
  events to keep the clip rect in sync with the actual client rect
  (`graphics::set_mouse_grab` / `refresh_mouse_grab` in
  `GameOS/gameos/gos_render.cpp`).

## Residual

On at least one 3-monitor setup the cursor can still escape by
**exactly one column of pixels** to the adjacent monitor without
losing focus. Cosmetic only — input continues to flow, gameplay
unaffected.

## Suspect causes (not yet confirmed)

Most likely **environmental**, not a code-side miss. Candidates:

- Another process calling `ClipCursor` (mouse-utility apps, gaming
  overlays, screen-corner activation tools) winning the race against
  ours.
- DPI seam between monitors of different scaling factors causing a
  one-pixel ambiguity in the rect calculation.
- Monitor-alignment utilities (DisplayFusion, UltraMon,
  PowerToys FancyZones) injecting their own cursor handling.

## Investigation moves if it ever bothers someone

- Reproduce on a clean Windows install (no monitor utilities, no
  gaming overlays) — if the escape vanishes, environmental confirmed,
  no code work needed.
- Log the actual rect we pass to `ClipCursor` against the rect Windows
  reports back via `GetClipCursor` — if they differ by 1 px on the
  trailing edge, the rect calc is the problem.
- Spy on `ClipCursor` calls in the process tree to see if another
  process is overwriting ours.

Low priority. Bug 3 (the dead-event-dispatch fix) was the real engine
fix in this branch and is independently valuable; the 1-px residual
is the long tail.
