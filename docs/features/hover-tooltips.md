# Hover Tooltips

Shipped: 2026-09-17 (commit `5f1970a`). No deviations.

Raygui tooltips on HUD/Settings controls plus a debounced world-space
unit tooltip; suppressed while gesturing, fog-gated for enemies.

## Behavior

- `GuiEnableTooltip` at startup; `GuiSetTooltip` on HUD and Settings
  controls (the menu half landed on Settings — `Menu.cpp` owns no
  widgets).
- Unit tooltip: per-frame `PickUnitAt`, 0.4s hover debounce, screen-space
  box; hidden during drag/place/attack-ground/area-repair/right-drag
  (state resets); enemies visible only when live-explored.
- Button tooltips are visual-only by design (no tests).

## Key files

- `src/game/public/app/Hud.h` — `HoverTooltipState`, line builders.
- `src/game/private/core/Game.cpp` — wiring, debounce, suppression,
  fog gate.
- Tests: `SelectionVisual` suite (debounce reset/threshold/hide).

## Decisions

- Suppression set is broader than the plan's minimum (any gesture
  hides the tooltip rather than fighting it).
