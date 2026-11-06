# Deploy Session Plan

Infantry production currently spawns a fully assembled squad at the bootcamp spawn tile
(`Simulation::PumpQueue` → `ProductionQueue::Update` → `UnitFactory::SpawnPrepaid`). Goal:
members exit the building door one by one, walk to their grid slots, and the squad becomes
controllable only when assembled. Delete this file when implementation is complete.

## Design decisions (locked)

- Deploying squads are **not targetable AND invulnerable** (belt and suspenders: targeting
  filter removes them from AI acquisition; the Combat-side guard is the authoritative shield
  for splash/force-attack paths).
- Deploying squads have **no occupancy**: they do not reserve footprints and do not block
  others; member walking resolves terrain only (occ pointer stays null-tolerant).
- Deploying squads are **uncontrollable**: selection, orders, AICommander, retreat all skip.
- Producer destroyed mid-deploy, evaluated on the same tick the building dies:
  - members already at their grid slots → survive, keep slots
  - members spawned with any walk progress → killed same tick (death burst, no kill credit)
  - members not yet spawned → never spawn; final member count shrinks permanently
  - survivors >= 1 → squad becomes controllable immediately with the reduced count
  - survivors == 0 → destroy the entity through the normal combat-death path (no attribution)
- Choreography is data-driven and type-independent: one shared exit-delay/walk-speed spec
  consumed by every infantry type; vehicles later join with member count = 1. Transport
  unload is the same shape (source passed in, not hardcoded to a building type).

## Verified touch points

| Concern | Location |
| --- | --- |
| Production spawn | `Simulation::PumpQueue` (`src/game/private/core/Simulation.cpp:288-338`), `ProductionQueue::Update` (`src/game/private/economy/Production.cpp:77-120`) |
| Entity creation | `UnitFactory::SpawnPrepaid` (`src/game/private/units/UnitFactory.cpp:29-61`) |
| Target acquisition | `AcquireTarget` filter (`src/game/private/units/Targeting.cpp:49-54`); in-flight revalidation around `Unit.cpp:257` |
| Area-attack splash victims | `ResolveGroundAttack` victim loop (`src/game/private/units/Combat.cpp:51-53`); shell-hit victim loop (`Combat.cpp:212-216`) |
| Direct damage | `ResolveAttack` (`src/game/private/units/Combat.cpp:16-29`) |
| Occupancy reserve | `src/game/private/units/Unit.cpp:336-345` |
| Selection | `isSelected` set in the select phase TU; consumption via `PlayingInput::SelectedUnits` (`src/game/private/core/PlayingInputOrders.cpp:177-187`) |
| Squad rendering / selection box | `SquadSlots` (`src/game/private/app/data/Art.cpp:33`), `SquadSelectionBox` (`src/game/private/core/GameRendererWorld.cpp:21`) |

## Data model

New component `Deployment` (`src/game/public/units/Deployment.h`), added only to deploying
entities, matching the `Orders`/`Mover`/`CombatState`/`Turret`/`Cargo` registry style. It
carries producer entity, membersTotal/membersSpawned/membersDied, spawn clock, exit delay,
door tile, and per-member walking positions. Assembling removes the component, so
`IsDeploying()` is a cheap `registry_.Get<Deployment>(id)` with nothing to forget to clear.

Config `data/configs/deploy.json` (loaded via `UnitConfigParse`/`UnitConfigCatalog`):
`exitDelay`, `walkSpeedMultiplier` per shared infantry spec — type independence lives here.

## Deploy session (new `units/Deploy` TU)

`src/game/public/units/Deploy.h` + `src/game/private/units/Deploy.cpp`. Pumped by
`Simulation` after `PumpProduction`.

- BeginDeploy: door tile via `NearestFreeFootprintTile`, position at door, membersTotal from
  `SquadSlots`, no occupancy reserve, no orders issued.
- UpdateDeploy: producer dead → CollapseDeploy; clock accumulates sim dt; spawn members at
  the door with a burst; walk members tile-step toward slots (terrain-only); when every
  spawned member is at its slot and membersSpawned == membersTotal → AssembleDeploy:
  remove component, footprint reserves normally from then on, rally order and
  `AutoAddGroupBit` application (moved from PumpQueue) fire here, Dispatch(DeployCompleted).
- Member walk deliberately bypasses `Orders`/`Mover` (those drive the whole entity and would
  fight the controllability gate); member positions live in the Deployment component and the
  renderer consumes them.

## Gate wiring

1. Targeting: `IsDeploying` skip beside `IsEmbarked` in `AcquireTarget`; check in the
   ValidateTarget path so an in-flight target drops when it starts deploying.
2. Combat (authoritative): skip Deployment victims in `ResolveGroundAttack` and the
   shell-victim loop; early-out in `ResolveAttack` when the defender carries Deployment.
3. Occupancy: skip the reserve at `Unit.cpp:336-345` for deploying entities;
   `ResolveStackedUnits` / `ResolveCrush` never see them (no reserved overlap).
4. Selection/orders: never set `isSelected` on a deploying entity.
5. AI: `AICommander::MaintainProduction` and combat assignments, `RetreatIfLowHP`, and
   `AssignAreaRepair` candidates all skip deploying entities.
6. Renderer: draw `membersSpawned` sprites at `Deployment.memberSpawnedPos` instead of the
   `SquadSlots` iteration; suppress selection box/badge; minimap dot stays on the door.
   Render member count is `min(deployment committed, squad count)` (SquadSlots derives from
   health; never index out of bounds).

## Save/load & events

`SaveEncode`/`SaveDecode`/`SaveWire` must round-trip the Deployment component (generation
stable entity ids) so quicksave mid-deploy reproduces member positions and the spawn clock.
`Event.h` gains `DeployCompleted`; SFX hooks attach to the event, not direct includes.

## Tests (tests/unit-tests/deploy_tests.cpp)

- assembly: member count grows; uncontrollable until assembled; controllable + reserved after.
- gates: enemy AcquireTarget never returns a deploying unit; splash and shell loops skip it;
  ResolveAttack no-ops; selection never picks it; AI never targets it.
- abort: building destroyed at walk progress 3/5 → spawned members killed, unspawned
  cancelled, slots survive, zero survivors destroys the entity through the normal path.
- door-trap gotcha: door tile via `NearestFreeFootprintTile`, never inside the footprint.
- repeat queue: simultaneous deploying squads each carry their own component.
- quicksave mid-deploy round-trip.
- integration-tests: add one squad-burst scenario; e2e unaffected (Tier-1 spawns directly).

## Milestones

1. **M1 — gate layer only, no choreography**: Deployment component + all filters wired; no
   producer creates the component yet, so behaviour is identical to today and every gate is
   provably inert (~ tests exercise it with synthetic components). Premake regen for the new
   test file.
2. **M2 — deploy session**: door exit, staggered member spawn, slot walk, assembly; rally
   order + auto control group move into AssembleDeploy. Bootcamp infantry only.
3. **M3 — combat + visuals**: death bursts, render wiring, damage number suppression,
   selection/badge suppression, SFX.
4. **M4 — abort rule** + tests.
5. **M5 — save/load round-trip + Workshop config entry** (vehicle exits, no slots; collapses
   if the workshop dies mid-roll) as the abstraction check.
6. **M6 — balance pass** vs `docs/BALANCE.md`: deployment adds effective delay to infantry
   availability; re-verify Easy/Medium/Hard AI pressure and Bootcamp 2.5s build feel.
