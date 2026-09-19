# Order Flags and Retreat Fallback (bugfix)

Shipped: fully, 2026-09-17 (commit `593f210`). Found in a post-ship audit
of the QoL work: two real, reachable bugs no test covered.

## Behavior

- Stale order flags: `ClearOrders` is public and runs on fresh single
  moves and inside both formation functions (queue-clear moved there
  too), so a repathed unit never acts on a previous order's flags.
  `RetreatIfLowHP` signature untouched.
- Retreat home: the map-center reference is replaced by an
  army-centroid resolver (`ResolvePlayerRetreatHome`), called once per
  tick before the team-0 retreat pass.

## Key files

- `src/game/public/units/Unit.h` — both declarations.
- `src/game/private/units/Unit.cpp` — `ClearOrders`, centroid resolver.
- `src/game/private/units/Formation.cpp`,
  `src/game/private/core/PlayingInput.cpp`,
  `src/game/private/core/Simulation.cpp` — call sites.
- Tests: `OrderQueue` (flag/queue clears), `Retreat` (multi-base,
  rally priority) suites.

## Decisions

- Centroid logic lives in `Unit.cpp` as a callable (not an inline
  `Game.cpp` block) so tests can drive it directly.
- Per-unit `homes[]` refinement explicitly deferred — not a bug.
