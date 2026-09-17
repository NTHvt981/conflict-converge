# Plan: Orders & Queueing QoL

## Implementation status: MOSTLY SHIPPED (updated 2026-09-17)

- Shipped: shift-queue chains (`2bf509c`), attack-ground (`6e758e3`),
  single-target repair baseline (`2f38d69`), area build incl. the
  single-placement flow (`bd6d105`).
- NOT shipped: the area-repair variant (§3a step 2 — needs
  `QueryBuildingsInRect` + nearest-first dispatch). Reclaim deferred
  per P9 (no wreck system), as decided.

Covers, from `missing_features.md` § "Orders & queueing":
- Shift-queued waypoint chains across order types (move → attack → patrol)
- Attack-ground / force-fire at a position
- Area commands (repair/reclaim/build pattern over a dragged zone)

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) first, especially **P3** (no order queue exists),
**P8** (no player-driven building placement), and **P9** (no wreck/reclaim system) — this plan
implements P3 and a minimal version of P8, and explicitly defers reclaim per P9.

---

## 0. The idiomatic "add a new order type" recipe (apply this to attack-ground)

Confirmed by tracing how Stance/Patrol/Repair were each added to `Unit`. Every new order type
follows the same 4-part shape — use this as a checklist for attack-ground below:

1. **Add fields to `Unit`** (`src/game/public/units/Unit.h`): a `bool hasX` flag + payload. E.g.
   Patrol added `hasPatrol`, `patrolA`/`patrolB`, `patrolToB`; Repair added `hasRepairOrder`,
   `repairTarget`.
2. **Add an `Issue*Order` free function** in `Unit.h`/`Unit.cpp` that (a) clears every *other*
   order type's fields so exactly one is ever active, (b) sets its own fields, (c) optionally
   chains into an existing order function for the movement leg (`IssuePatrolOrder` chains into
   `IssuePathOrder`; `IssueRepairOrder` chains into `StopMoving`).
3. **Add a branch inside `UpdateUnit`** (`Unit.cpp`) at the correct priority tier. Current order:
   repair (highest) → move/path/attack-move → plain acquire/idle. A new order type slots in near
   whichever existing type it most resembles behaviorally.
4. **Add a keybinding** in `Game::BindShortcuts` (`src/game/private/core/Game.cpp:192-364`),
   following the `KEY_A`/`KEY_V` shape (guard on `worldActive && menu.state==Playing`, get the
   unit(s), call the new `Issue*Order`, play `SfxId::Confirm`).
5. **Add a clear-line** in the `KEY_SPACE` halt handler (`Game.cpp:341-363`) so `Space` still
   cancels the new order type.
6. **Add a `RunXTests()` file** mirroring `formation_tests.cpp`'s shape, registered in
   `tests/main.cpp`.

**Known pre-existing gotcha to fix while you're in this area, not just imitate:** `KEY_A`/`KEY_H`/
`KEY_G`/`KEY_V` in `BindShortcuts` all call `SelectedUnit(registry)` and act on **exactly one
unit** — the first one iteration order happens to find — even when multiple units are selected.
Only mouse-driven right-click orders route through the full squad + formation-fan-out path
(`Game.cpp:645-664`). This means today, selecting 5 tanks and pressing `A` only attack-moves one of
them. New keyboard-triggered order types added by this plan (queue-then-fire, attack-ground if
keybound) should use the squad-scan idiom, not `SelectedUnit`, and you should consider fixing the
existing four shortcuts to match while you're touching this code — flag it as a deliberate,
separate commit if you do, since it's a behavior change to existing shortcuts, not purely additive.

---

## 1. Shift-queued waypoint chains across order types

### Confirmed constraint
`Unit` supports exactly **one** pending order at a time, of any type. `moveTarget/hasMoveOrder`,
`attackMove/attackMoveDest`, `hasPatrol/...`, `hasRepairOrder/repairTarget` are mutually exclusive
single slots; every `Issue*Order` stomps the others. The `path`/`pathNext`/`hasPath` triplet is the
A* waypoint list *for the current single order*, not a queue of *orders* — don't conflate the two.

### Data model
Add to `Unit.h`:
```cpp
// A single queued order, dispatched via the same Issue* functions once the
// current order completes. `kind` selects which payload fields are valid;
// unused fields are ignored (e.g. Patrol uses pointA/pointB, Move uses only
// pointA, Repair uses target).
enum class QueuedOrderKind
{
    Move,
    AttackMove,
    Patrol,
    Repair,
    AttackGround, // only meaningful once §2 below lands
    Count
};

struct QueuedOrder
{
    QueuedOrderKind kind = QueuedOrderKind::Move;
    Vector2 pointA = {};
    Vector2 pointB = {}; // Patrol's second waypoint
    Entity target = kInvalidEntity; // Repair's target
};
```
Add `std::vector<QueuedOrder> orderQueue;` to `Unit`, near the `path`/`hasPath` block.

**Do not add this to `SaveGame.proto` in this pass** — same reasoning as control groups (plan 1):
keep the schema change out of scope; queued orders not surviving a quicksave is an acceptable,
documented limitation for v1 (note it in a comment on the field).

### Issuing into the queue vs. immediately
Add a single dispatch point rather than teaching every call site about Shift:
```cpp
// If shiftQueue is false: clears orderQueue and issues immediately (today's
// behavior, unchanged). If true: appends to orderQueue instead of issuing
// now (unless the unit is currently idle with an empty queue, in which case
// it issues immediately — queueing an order on an idle unit should still
// start it right away, matching genre convention).
void IssueOrEnqueue(Unit &unit, const TileMap &map, const OccupancyGrid *occ,
                    Entity self, std::uint32_t selfGen, bool shiftQueue,
                    QueuedOrder order);
```
Implementation dispatches `order.kind` to the corresponding existing `Issue*Order` function when
issuing immediately, or just `push_back`s onto `orderQueue` when queueing. This needs `map`/`occ`
in scope for the footprint-aware variants — call it from `Game.cpp`'s order-issuing sites (which
already have `map`/`occ`), not from deep inside `UpdateUnitMovement`.

### Dequeue-on-completion
There's no single "order finished" hook (see roadmap P2) — add the dequeue check at the specific
points where an order currently transitions to "finished, nothing pending":
1. Wherever `UpdateUnitMovement`'s arrival path currently clears `hasMoveOrder`/`hasPath` (final
   arrival, not intermediate waypoints) — after clearing, check `if (!unit.orderQueue.empty())`,
   pop the front `QueuedOrder`, and dispatch it through the same `Issue*Order` switch used by
   `IssueOrEnqueue` above. This dispatch needs `map`/`occ`, which `UpdateUnitMovement` doesn't
   currently take — either thread them through (it already takes `OccupancyGrid *occ` per its
   existing signature, just add a `const TileMap &map` if not already present — check the current
   signature, it likely already has `map` since occupancy checks need it) or do the dequeue check
   one level up, in `UpdateUnit` (`Unit.cpp`, which already has both `map` and `occ` in scope) right
   after calling `UpdateUnitMovement`, by checking "did this call just transition from
   `hasMoveOrder||hasPath` true to false."
2. Repair-order completion (target destroyed/healed to full, wherever that's currently detected in
   `Unit.cpp`'s repair branch) — same dequeue-and-dispatch.
3. Patrol doesn't "complete" (it loops `patrolToB`) — a queued order after a patrol order only
   makes sense if something explicitly ends the patrol (e.g. a new order overwrites it, which
   already happens via `SetStance`/a fresh `Issue*Order`) — no dequeue hook needed for patrol
   itself, but *arriving* at a patrol leg should NOT trigger dequeue (patrol never "completes").
   Make sure the dequeue check is scoped to only fire when an order genuinely ends (arrival with no
   loop, repair-target-lost, or explicit cancel) not on every waypoint arrival within a multi-leg
   order.
4. `CancelAtBlocked` (retry-budget-exhausted cancel) — decide explicitly whether a cancelled order
   should still advance the queue (recommend: **no**, drop the rest of the queue too, since the
   unit is stuck and blindly marching into the next queued order it also can't reach is worse UX
   than stopping and waiting for a new player command) or advance anyway (simpler code, matches
   "just keep trying the list"). Document whichever you pick in a comment on `orderQueue`.

### Keyboard shortcuts must go through squad, not `SelectedUnit`
Per §0's flagged gotcha: `KEY_A`/`KEY_V`'s handlers in `BindShortcuts` need to iterate the full
selected squad (mirror `Game.cpp:645-651`'s scan) and call `IssueOrEnqueue` per unit with
`shiftQueue = input.ShiftDown()`, not act on a single `SelectedUnit(registry)` result — queueing
across a multi-unit selection only makes sense group-wide.

### Right-click (mouse) queueing
`Game.cpp`'s right-click handler (`Game.cpp:641-667`) already branches on `squad.size()`. Change
both the single-unit and formation branches to check `input.ShiftDown()`:
- If not held: today's behavior exactly (`IssuePathOrderFootprint` / `formation::
  IssueFormationMoveFP`), which should also clear `orderQueue` for every affected unit (a
  non-queued order always replaces everything pending).
- If held: build a `QueuedOrder{Move, worldPos}` (or `AttackMove` if attack-move mode is active —
  check whatever toggles attack-move mode for right-click today) per unit and call
  `IssueOrEnqueue(..., shiftQueue=true, ...)` instead.

### Tests
New `tests/unit-tests/order_queue_tests.cpp` (`RunOrderQueueTests`): construct a unit, issue a
Move order, Shift-queue an AttackMove and a Patrol, walk frames until the first order completes,
assert the queue dequeues the AttackMove next (fields match), then repeat until it also completes
and the Patrol becomes active. Also test: queueing on a fully-idle unit issues immediately (queue
stays empty). Also test: `Space` (or whatever clears orders) empties `orderQueue` too — grep the
halt handler and add the clear there as part of this feature.

---

## 2. Attack-ground / force-fire at a position

### Closest existing analogues (repair order's approach-then-channel shape is the template)
`IssueRepairOrder`/its `UpdateUnit` branch already implement "approach if out of range, else
channel a per-frame effect" — attack-ground is structurally the same, with "channel = deal damage"
instead of "channel = heal," and a fixed `Vector2` instead of an `Entity` target.

### Data model
Add to `Unit.h` (near `hasRepairOrder`/`repairTarget`):
```cpp
bool hasAttackGroundOrder = false;
Vector2 attackGroundPos = {};
```

### Resolution against empty ground
`Combat.h`'s `ResolveAttack`/`ResolveBuildingAttack` are strictly unit-vs-unit or unit-vs-building
— there's no "resolve against a point" function. Add one, reusing the existing
`QueryUnitsInRect(Registry&, Rectangle, int teamID, std::vector<Entity>&)` (`Combat.h:59`):
```cpp
// Fires `attacker`'s attack at a world position: builds a small rect
// (radius = attacker.attackRange or a fixed splash radius, whichever the
// design calls for) around `pos`, finds enemy units in it via
// QueryUnitsInRect, and applies ResolveAttack to each (or the single
// nearest one, if splash isn't wanted — pick one explicitly, don't leave
// it ambiguous). No-op (attacker still "fires," just hits nothing) if the
// area is empty, matching genre convention that attack-ground can miss.
void ResolveGroundAttack(Registry &registry, Unit &attacker, Vector2 pos);
```
Add to `Combat.h`/`Combat.cpp`. Decide splash-vs-single-target explicitly in the implementation
comment — recommend single-target (nearest enemy within a small radius of `pos`) for a first cut,
since true splash damage (partial falloff by distance) is a separate balance feature not requested
here.

### `Issue*Order` function
```cpp
void IssueAttackGroundOrder(Unit &unit, const TileMap &map, Vector2 worldPos);
// Footprint-aware variant following IssueAttackMoveOrderFootprint's shape,
// if a footprint-aware march-to-range leg is wanted:
void IssueAttackGroundOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                     Vector2 worldPos, Entity self, std::uint32_t selfGen);
```
Clears every other order's fields (mirror `IssueRepairOrder`'s clear list at the top), sets
`hasAttackGroundOrder = true; attackGroundPos = worldPos;`, and issues a path order toward
`worldPos` if outside `attackRange` (reuse `IssuePathOrder`/`IssuePathOrderFootprint` — same
"approach" leg every other order type already uses).

### `UpdateUnit` branch
Add a new tier, positioned similarly to the repair-order check (early, since it's a deliberate
player order that should take priority over passive target-acquisition): if
`unit->hasAttackGroundOrder`, compute distance to `attackGroundPos`; if outside `attackRange`,
drive movement toward it (reuse the existing "approach" pattern from the repair branch); if inside
range, call `ResolveGroundAttack(registry, *unit, unit->attackGroundPos)` gated on the unit's
existing `AttackPhase`/cooldown machinery (`UpdateAttackPhases`/`UpdateAttack`, same phase-gating
every other attack path already uses — don't bypass windup/cooldown for ground attacks).
Attack-ground should **not** auto-cancel when nothing's there to hit — it's a standing "keep
shelling this spot" order until the player issues a new one or cancels; decide and document whether
it's one-shot (fire once, then clear) or continuous (keep firing on cooldown until cancelled) —
recommend continuous, matching "force-fire" semantics from the feature name.

### Input
Right-click already means move/attack-move. Attack-ground needs a modifier, but Ctrl is reserved
for control groups (plan 1) and the plain-drag-modifier-key is reserved for line formation
(plan 1 §3, e.g. Alt). Recommend a distinct dedicated key held during right-click, or a toggled
"attack-ground mode" activated by a keybinding (press a key, next right-click is interpreted as
attack-ground, mode clears after one use) — the latter avoids needing a second simultaneously-held
modifier and matches how some RTS UIs implement force-fire. Pick one and wire it into `Game.cpp`'s
right-click block (`Game.cpp:641-667`), using the squad-scan idiom (not `SelectedUnit`) so it works
correctly for group selections.

### Tests
New block in `combat_tests.cpp` or a new `attack_ground_tests.cpp`: single unit, empty ground,
issue attack-ground order at an out-of-range point, walk frames, assert it approaches then holds
at range; place an enemy unit near the target point, walk frames past a windup+cooldown cycle,
assert the enemy took damage. Also test the "no enemy present" case resolves without crashing and
the unit keeps standing/firing (or clears, per whichever one-shot-vs-continuous decision was made).

---

## 3. Area commands (repair/reclaim/build over a dragged zone)

**This is the largest-gap feature in the whole roadmap** — confirmed near-zero existing foundation
for two of its three sub-parts. Scope explicitly; don't try to land all three in one pass.

### 3a. Area repair
`IssueRepairOrder(Unit&, Entity)` exists but is single-target, `Entity`-keyed, and **has no
keybinding or mouse gesture wired to it anywhere today** — even the base single-target repair
command isn't player-triggerable yet. This plan must add both:
1. **Baseline**: a right-click-on-a-damaged-unit-or-building-while-an-Engineer-is-selected command
   issuing `IssueRepairOrder` (single target, existing function) — a small, independent addition,
   should probably ship on its own before area-repair, since area-repair without single-target
   repair working is untestable/unverifiable in isolation.
2. **Area variant**: reuse the drag-box gesture (§ line formation in plan 1 established a
   right-drag pattern with a modifier; area-repair likely wants its own modifier or a dedicated
   "repair mode" toggle similar to attack-ground's mode-toggle idea above — don't stack too many
   modifiers onto plain right-drag, pick distinct explicit triggers per feature and document the
   full keybinding table in one place once all these land). On release, use `DraggedWorldBox`
   (plan 1 §0.3) to get the world rect, then:
   - Collect damaged same-team units in the rect via a rect query (extend `QueryUnitsInRect`'s
     existing shape or reuse it directly, filtering `health < BaseStats(type).health`).
   - Collect damaged same-team buildings in the rect — **no `QueryBuildingsInRect` exists**; add
     one in `Combat.h/.cpp` or `Building.h/.cpp` mirroring `QueryUnitsInRect`'s shape but scanning
     `registry.Each<Building>` filtered by rect + `health < maxHealth`.
   - For each selected Engineer in the current squad, assign it to its nearest damaged target
     (simple greedy nearest-unassigned-target loop is sufficient — this isn't a hard assignment
     problem worth optimizing for a QoL feature) and call `IssueRepairOrder`.

### 3b. Area build (pattern placement over a dragged zone)
**No player-driven building placement exists at all** — `PlaceBuilding` (`Building.h:60-63`) is
only ever called from AI/skirmish setup, never in response to player input in `Game.cpp`. This
sub-feature requires building the base flow first:
1. **Baseline single-placement flow** (new, not a QoL wrapper around something existing):
   - A "placement mode" state on `Game` (e.g. `std::optional<BuildingType> placingType`), entered
     via a new production-panel button or hotkey (reuse `Hud.cpp`'s existing button-drawing
     pattern from `DrawProductionPanel`, `Hud.cpp:128-175`, for a "Place: <Building>" button set).
   - Per-frame ghost rendering: while `placingType` is set, draw a translucent footprint outline at
     the current mouse's snapped tile (footprint size via `Footprint(type)`, `Building.h:51`),
     tinted green/red based on whether `PlaceBuilding`'s validation would currently succeed there
     (you may need to expose a `bool CanPlaceBuilding(...)` predicate that `PlaceBuilding` already
     computes internally — factor it out so the ghost preview can call it without side effects).
   - On left-click while `placingType` is set: call `PlaceBuilding(registry, map, *placingType,
     teamID, tileX, tileY, &nodes)`; on success clear `placingType` (or keep it active for
     multi-place if that's the desired UX — decide explicitly); on right-click or Escape, cancel
     placement mode without placing.
2. **Area/pattern variant**: once single-placement exists, dragging a rect instead of a single
   click tiles candidate positions across the dragged area (spaced by `Footprint(type)`, skipping
   any tile where `CanPlaceBuilding` fails) and calls `PlaceBuilding` per valid slot. This is a
   thin wrapper around the baseline flow — don't build it before the baseline exists.

**Recommend treating 3b as a separate, larger follow-up PR from 3a** given the amount of new
surface area (an entire placement-mode UI state machine) versus 3a's much smaller lift on top of
existing repair infrastructure.

### 3c. Area reclaim — explicitly deferred
No wreck/salvage entity system exists anywhere in the codebase; unit/building death never spawns a
reclaimable wreck. This sub-feature has no foundation to build on and needs its own design (new
entity type, death hook to spawn wrecks, a reclaim order type, a resource-yield formula) before
"area reclaim" is even meaningful. **Do not attempt to implement this as part of the area-commands
pass** — track it as a separate future feature contingent on a wreck-system design decision.

### Tests
`building_tests.cpp`: extend with `CanPlaceBuilding` predicate tests (valid tile, occupied tile,
terrain-blocked tile, out-of-bounds). New `area_repair_tests.cpp` or extend `building_tests.cpp`:
construct several damaged units/buildings in and out of a drag rect, call the area-repair
dispatch function, assert only in-rect damaged targets got `IssueRepairOrder`'d and assignment was
nearest-first.
