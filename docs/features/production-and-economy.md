# Production and Economy QoL

Shipped: fully shipped 2026-09-17.

Repeat production queues, tab-filtered production panel, and select-all
hotkeys around build and placement flows.

## Behavior

- Repeat queue: per-type `R`/`R*` toggle rearms the item in place;
  broke queues park at 100% and resume when affordable; cancelling
  refunds. Repeat-arm state lives on `ProductionQueue` (stateless panel).
- Split queues: the Bootcamp queue takes infantry, the Workshop queue
  takes vehicles; each bound queue rejects the other category. Each
  queue only advances while its own producer stands.
- Production tabs: the infantry tab drives the Bootcamp queue, the
  vehicles tab drives the Workshop queue, and the building tab drives
  neither (`SelectPlacingType` enters placement mode per type). Unit
  tabs show only while the matching live producer stands. Selecting a
  producer switches to its tab.
- `C` selects all of type, `F` selects all production (highlight ring +
  count, no command UI — intentional).
- Per-unit production hotkeys (queue by keyboard) were never scoped and
  are still absent; the panel is mouse-driven.

## Key files

- `src/game/public/economy/Production.h` + `private/economy/Production.cpp` —
  queues, `Item::repeat`, re-charge/park, cancel-refund, category binding.
- `src/game/public/app/Selection.h` + `private/app/Selection.cpp` —
  `SelectAllOfType(+InRect)`, `SelectAllBuildings`, `SelectAllProductionBuildings`.
- `src/game/private/app/Hud.cpp` — production category orders and the
  abilities selection query.
- `src/game/private/app/RmlUiHud.cpp` — tab switching, per-tab enqueue /
  repeat, building placement entry, ability buttons.
- Tests: `Production` (repeat/park/resume/refund, category binding), `Selection` (type,
   in-rect), `Building` (producers), `Hotkey` (remap contract),
   `UnitCommands` (armed abilities).

## Decisions

- Repeat state on the queue, not `Game` (immediate-mode panel stays
  stateless).
- Open: per-unit-type build hotkeys (only select-all-production exists).
