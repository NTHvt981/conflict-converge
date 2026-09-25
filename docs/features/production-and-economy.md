# Production and Economy QoL

Shipped: fully shipped 2026-09-17.

Repeat production queues, tab-filtered factory panel, and select-all
hotkeys around build and placement flows.

## Behavior

- Repeat queue: per-type `R`/`R*` toggle rearms the item in place;
  broke queues park at 100% and resume when affordable; cancelling
  refunds. Repeat-arm state lives on `ProductionQueue` (stateless panel).
- Factory tabs: infantry / vehicle unit lists plus a building tab that
  enters placement mode per type (`SelectPlacingType`); unit tabs show
  only while a live Factory stands.
- `C` selects all of type, `F` selects all factories (highlight ring +
  count, no command UI — intentional).
- Per-unit production hotkeys (queue by keyboard) were never scoped and
  are still absent; the panel is mouse-driven.

## Key files

- `src/game/public/economy/Production.h` + `private/economy/Production.cpp` —
  queue, `Item::repeat`, re-charge/park, cancel-refund.
- `src/game/public/app/Selection.h` + `private/app/Selection.cpp` —
  `SelectAllOfType(+InRect)`, `SelectAllBuildings`.
- `src/game/private/app/Hud.cpp` — production category orders and the
  abilities selection query.
- `src/game/private/app/RmlUiHud.cpp` — tab switching, per-tab enqueue /
  repeat, building placement entry, ability buttons.
- Tests: `Production` (repeat/park/resume/refund), `Selection` (type,
  in-rect), `Building` (factories), `Hotkey` (remap contract),
  `UnitCommands` (armed abilities).

## Decisions

- Repeat state on the queue, not `Game` (immediate-mode panel stays
  stateless).
- Open: per-unit-type build hotkeys (only select-all-production exists).
