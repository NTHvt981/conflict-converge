# Orders and Queueing

Shipped: umbrella mostly shipped 2026-09-17, including the extracted
area-repair follow-up (`AreaRepair_Plan.md`, commit `42c3556`). Reclaim
was explicitly deferred (roadmap P9) and is still absent — there is no
wreck/salvage system.

Shift-queued order chains, attack-ground, patrol semantics, single-target
and area repair, and area-build.

## Behavior

- `Shift` queues Move/AttackMove/Patrol/Repair/AttackGround chains
  (`Unit.orderQueue`); `Space` clears. Patrol never dequeues (loops);
  a cancelled order drops the whole queue (documented anti-stuck rule).
- Attack-ground (`X`): continuous shelling with single-nearest splash
  (`ResolveGroundAttack`), not one-shot.
- Single-target repair (Engineers, right-click) with cursor hint and
  repairability checks; area-build (`Z`) and area-repair (`E`) drag modes
  resolve through `DraggedWorldBox`, with live ghost preview and
  per-Engineer `Shift` queueing for repair.
- Modes live in `PlayingInput` behind remappable actions; placement
  cancel takes precedence over line-formation.

## Key files

- `src/game/public/units/Unit.h` + `private/units/Unit.cpp` — orders,
  queue dispatch (`DispatchQueuedOrder`, `OnOrderFinished/Cancelled`),
  repair branches, pure area-repair helpers.
- `src/game/public/units/Combat.h` — `ResolveGroundAttack`.
- `src/game/public/economy/Building.h` — footprint rect, building rect
  query, area-build slots.
- `src/game/public/core/PlayingInput.h` — drag-mode state and toggles.
- Tests: `OrderQueue`, `AttackGround`, `AreaRepair`, `Building` suites.

## Decisions

- Area-repair is a left-drag sibling of area-build (key `E`), not
  right-drag; the cancel chain keeps `Alt` reserved for line-formation.
- Repair dispatch reuses the ordinary per-unit order path (`Shift`
  extends), so queued repairs behave like any other order.
- Open: reclaim (wrecks/salvage) — deferred by roadmap decision, no
  symbols exist yet.
