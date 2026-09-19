# Production and Economy QoL

Shipped: fully shipped 2026-09-17.

Repeat production queues, idle-finder buttons, and select-all hotkeys
around the factory panel.

## Behavior

- Repeat queue: per-type `R`/`R*` toggle rearms the item in place;
  broke queues park at 100% and resume when affordable; cancelling
  refunds. Repeat-arm state lives on `ProductionQueue` (stateless panel).
- Idle buttons: select-idle-workers / select-idle-army with live counts
  (greyed at 0, replace semantics); channeling Engineers don't count
  as idle.
- `C` selects all of type, `F` selects all factories (highlight ring +
  count, no command UI — intentional).
- Per-unit production hotkeys (queue by keyboard) were never scoped and
  are still absent; the panel is mouse-driven.

## Key files

- `src/game/public/economy/Production.h` + `private/economy/Production.cpp` —
  queue, `Item::repeat`, re-charge/park, cancel-refund.
- `src/game/public/app/Selection.h` + `private/app/Selection.cpp` —
  `SelectIdle`/`CountIdle`, `SelectAllOfType(+InRect)`,
  `SelectAllBuildings`.
- `src/game/private/app/Hud.cpp` — idle buttons, production panel with
  `R*` toggles, building count.
- Tests: `Production` (repeat/park/resume/refund), `Selection` (idle,
  type, in-rect), `Building` (factories), `Hotkey` (remap contract).

## Decisions

- Repeat state on the queue, not `Game` (immediate-mode panel stays
  stateless).
- Idle camera-jump alerts stayed out of scope — buttons select only.
- Open: per-unit-type build hotkeys (only select-all-production exists).
