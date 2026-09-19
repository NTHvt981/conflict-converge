# Building Construction

Shipped: 2026-09-17 (commit `0381249`). No deviations.

Placed buildings start as sites at 0 HP, ramp to full over per-type
times, then go operational — with a world-space progress bar.

## Behavior

- `PlaceBuilding` starts `UnderConstruction`; `UpdateBuildingConstruction`
  ramps health proportionally (Base 3s / Depot 2s / Factory 4s), flipping
  to `Operational` at exact max. Ticked every sim step.
- Income, targeting, and repair all gate on `== Operational`, unchanged.
- Loaded sites restart at 0 HP (construction timer isn't serialized);
  legacy zero-HP saves heal to full (except `Destroyed`, which stays rubble).
- Progress bar drawn in world space; sprite-dimming cue not taken.

## Key files

- `src/game/public/economy/Building.h` — states, `constructionTime`,
  `BuildingBuildTime`.
- `src/game/private/economy/Building.cpp` — place/ramp/times.
- `src/game/private/core/Simulation.cpp` — per-tick update.
- Tests: `Building` suite; construction ticks also covered via AI,
  integration, map, skirmish, orders, and save tests.

## Decisions

- `UnderConstruction` appended AFTER `Destroyed` so wire values 0/1
  decode like legacy saves (save-compat by enum order).
- Short build times: a 12s factory broke the soak ladder.
