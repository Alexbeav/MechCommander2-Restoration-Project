# Auto-detect display resolution on first launch

Status: open, small task.

The shipped `options.cfg` hardcodes `ResolutionX = 1920` /
`ResolutionY = 1080`. On any monitor that isn't exactly that
resolution, the game launches at the hardcoded size rather than
matching the user's display.

## Desired behavior

On first launch (no valid options.cfg, or a sentinel value), query the
current primary display resolution via SDL
(`SDL_GetCurrentDisplayMode` returns the width/height of the active
desktop mode) and write those values to options.cfg. Subsequent
launches read whatever the user last saved — the options menu still
lets them change it.

## Implementation pointer

Likely touch point: wherever options.cfg is first loaded / initialized
(search `ResolutionX` in `code/` and `gui/` — a
`readIdLong("ResolutionX", ...)` path is the natural hook). Write the
detected value only when the key is missing, so user-set values are
preserved.
