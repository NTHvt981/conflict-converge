# Plan: Fix two bugs found while auditing the QoL plans

Two real, currently-reachable bugs were found while verifying the shipped QoL work against
`plans/QoL_*.md`. Both are isolated, small fixes. Full test suite is green (3447/3447) and both
soak trajectories are unaffected today — these bugs are in paths the existing tests don't exercise
(a stale-flag interaction and a multi-base scenario), which is exactly why they slipped through.

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) for the surrounding architecture conventions
(order-type recipe, testing split between pure logic and draw calls) before starting — both fixes
below follow those same conventions.

---

## Bug 1: stale order-type flags survive a direct mouse move order

### Root cause
`Unit` enforces "exactly one order active at a time" via a shared helper:

```cpp
// src/game/private/units/Unit.cpp:355-363 (anonymous namespace, file-private today)
void ClearOrders(Unit &unit)
{
    unit.attackMove = false;
    unit.hasPatrol = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
    unit.hasAttackGroundOrder = false;
    unit.speedCapPixelsPerSec = -1.0f; // QoL: caps never leak across orders
}
```

Every `Issue*Order` wrapper (`IssueMoveOrder`, `IssueAttackMoveOrder(Footprint)`,
`IssuePatrolOrder`, `IssueRepairOrder`, `IssueAttackGroundOrder(Footprint)`, and the queued-order
dispatcher) calls this before setting its own flag. But the **mouse-driven direct order path**
does not go through any of those wrappers — it calls the low-level `IssuePathOrderFootprint`
straight from `Game.cpp`, and a comment documents this as a deliberate (but incomplete) shortcut:

> `src/game/public/units/Unit.h:274-276`: *"plain right-click keeps its legacy direct
> IssuePathOrderFootprint call (plus a queue clear) and only routes Shift+right-click through
> here."*

Three call sites take this shortcut and only clear part of what `ClearOrders` clears:

1. **Single-unit direct move** — `src/game/private/core/Game.cpp:1163-1173`:
   ```cpp
   else
   {
       ordered->orderQueue.clear();
       // Fresh single order (not formation): drop any
       // stale group cap — IssuePathOrder* doesn't route
       // through ClearOrders like the Issue* wrappers.
       ordered->speedCapPixelsPerSec = -1.0f;
       IssuePathOrderFootprint(*ordered, map, occ,
                               input.MouseWorld(camera), squad[0],
                               registry.Generation(squad[0]));
   }
   ```
   Clears the queue and the speed cap, but not `hasRepairOrder`/`repairTarget`/
   `hasAttackGroundOrder`/`hasPatrol`/`attackMove`.

2. **Squad formation move** — `src/game/private/core/Game.cpp:1195-1207`:
   ```cpp
   for (const Entity id : squad)
   {
       if (Unit *unit = registry.Get<Unit>(id))
       {
           unit->orderQueue.clear();
       }
   }
   formation::IssueFormationMoveFP(registry, squad, map, occ,
                                   input.MouseWorld(camera),
                                   moveAtSlowestSpeed);
   ```
   Only clears the queue. `formation::IssueFormationMoveFP`
   (`src/game/private/units/Formation.cpp:63-111`) itself only ever touches
   `speedCapPixelsPerSec` (line 102) before calling `IssuePathOrderFootprint` — it never clears
   the other flags either.

3. **Alt-drag line formation** — `src/game/private/core/Game.cpp:1236-1254`, dispatching to
   `formation::IssueLineFormationMoveFP` (`Formation.cpp:138-176`). This one is worse: it doesn't
   even clear `orderQueue` (unlike the other two), on top of not clearing the order-type flags.

### Why this matters (verified against `UpdateUnit`'s priority order, `Unit.cpp:581-642`)
`hasRepairOrder` and `hasAttackGroundOrder` are checked and the function **returns** before the
`hasMoveOrder || hasPath` block is ever reached. So today:

- Right-click a repairing Engineer onto open ground (no Shift) → `hasMoveOrder`/`hasPath` get set,
  but the driver never reaches them — **the Engineer keeps repairing its old target**, and the
  player's move order is silently swallowed.
- Same failure for a unit mid attack-ground order (`hasAttackGroundOrder`) given a plain move: it
  **keeps shelling the old spot**.
- A patrolling unit given a plain move detours correctly while `hasMoveOrder`/`hasPath` are set
  (patrol's own re-trigger at `Unit.cpp:734-735` is gated on those), but once it **arrives**,
  `hasPatrol` is still `true` and it **resumes the old patrol route** instead of stopping —
  breaking the "exactly one order" invariant every other order type in this codebase relies on.

None of `order_queue_tests.cpp` / `attack_ground_tests.cpp` / `formation_tests.cpp` exercise a
unit that already has a conflicting order flag set when a fresh mouse order arrives, so this gap
has no test coverage today.

### Fix

**Step 1 — export `ClearOrders` as part of the public API**, so `Game.cpp` and `Formation.cpp` can
call it instead of hand-rolling a partial clear list (which is exactly how this bug happened —
two of the three call sites hand-rolled an incomplete subset):

- In `src/game/private/units/Unit.cpp`, remove the anonymous-namespace wrapping around `ClearOrders`
  (delete the forward declaration inside the anonymous namespace at line 21 and the function's own
  anonymous-namespace wrapping at its definition, lines 349-365 — check the surrounding
  `namespace { ... }` block's other contents before deleting the braces outright, since
  `RepairAim`/other private helpers may share it).
- Add a public declaration to `src/game/public/units/Unit.h`, near the other order functions (e.g.
  right before `IssueMoveOrder` at line 218), with a doc comment explaining it's now also meant to
  be called directly by any code issuing a fresh, non-queued order outside the `Issue*Order`
  wrappers:
  ```cpp
  // Shared clear: exactly one order active at a time. Every Issue*Order
  // wrapper calls this before setting its own flag. Call it directly
  // (before IssuePathOrderFootprint / formation::Issue*FormationMoveFP)
  // wherever a fresh, non-queued order bypasses those wrappers -- e.g. the
  // direct mouse-order dispatch in Game.cpp -- so a leftover repair/
  // attack-ground/patrol flag can't silently swallow the new order (see
  // plans/Bugfix_Order_Flags_And_Retreat_Fallback_Plan.md).
  void ClearOrders(Unit &unit);
  ```
- Do **not** fold this call into `IssuePathOrderFootprint`/`IssuePathOrder` themselves — those are
  also the movement leg used *by* `IssueAttackMoveOrderFootprint`, `IssueAttackGroundOrderFootprint`,
  and the repair-approach path, each of which calls `ClearOrders` first and then sets its *own*
  flag before calling the path function. If the path function also called `ClearOrders`, it would
  immediately wipe the flag that was just set. `ClearOrders` must stay something the *caller*
  invokes once, at the top of a fresh-order dispatch — never inside the shared movement primitives.

**Step 2 — call it at the three gap sites:**

1. `Game.cpp:1163-1173` (single-unit branch): replace the manual
   `ordered->speedCapPixelsPerSec = -1.0f;` line with `ClearOrders(*ordered);` (it already resets
   the speed cap, so this is a straight swap, not an addition) — keep the existing
   `ordered->orderQueue.clear();` line as-is, `ClearOrders` doesn't touch the queue.
2. `Formation.cpp:63-111` (`IssueFormationMoveFP`): add `ClearOrders(*unit);` immediately before
   the existing `unit->speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;` line (92-111 loop)
   — `ClearOrders` will reset the cap to `-1.0f` first, then the existing line overwrites it with
   the real value if `slowestSpeed` is set, so ordering is safe either way, but put `ClearOrders`
   first for readability (matches the `Issue*Order` wrappers' own ordering).
3. `Formation.cpp:138-176` (`IssueLineFormationMoveFP`): same addition, immediately before its
   `unit->speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;` line (~line 167). **Also** add
   `unit->orderQueue.clear();` in the same loop — this function is missing that too (see below).

**Step 3 — fix the missing queue-clear on line formation.** Unlike the squad-formation branch
(`Game.cpp:1197-1203`, which clears each unit's `orderQueue` before calling
`IssueFormationMoveFP`), the Alt-drag line-formation call site
(`Game.cpp:1236-1254`) never clears `orderQueue` at all before calling
`IssueLineFormationMoveFP`. Recommended: move the clear *into* `IssueLineFormationMoveFP` itself
(alongside the `ClearOrders` call added in Step 2.3) rather than adding it at the Game.cpp call
site, so the function is self-contained and can't have this gap reintroduced by some future
caller. Optionally (not required — just consider it while you're in this code) do the same for
`IssueFormationMoveFP` and drop the now-redundant per-unit clear loop at `Game.cpp:1197-1203`, so
both formation functions handle their own "fresh order" bookkeeping consistently instead of one
relying on its caller and the other not.

### Tests to add
Extend `tests/unit-tests/order_queue_tests.cpp` (or a new focused block in the same file) with
cases that were missing before:
- A repairing Engineer (`hasRepairOrder = true`) receives a plain single-unit right-click-style
  move order (call the same code path the fix touches — i.e. construct the scenario as close to
  `Game.cpp`'s call as the test harness allows, or at minimum call `ClearOrders` + the path-order
  function in the same sequence and assert `hasRepairOrder` is now `false` and `hasMoveOrder`/
  `hasPath` reflect the new order).
- A unit with `hasAttackGroundOrder = true` receives a fresh squad-formation move via
  `formation::IssueFormationMoveFP`; assert `hasAttackGroundOrder` is `false` afterward and the
  unit is actually moving toward the new destination, not still shelling the old spot.
- A patrolling unit (`hasPatrol = true`) receives a line-formation move via
  `IssueLineFormationMoveFP`; assert `hasPatrol` is `false` afterward (not just that it detours
  correctly mid-flight, which already worked before this fix — the bug is specifically that it
  used to *resume* patrolling after arrival).
- A unit with a non-empty `orderQueue` receives a line-formation move; assert the queue is now
  empty (covers the Step 3 fix independently of the flag-clearing fix).

Run the full suite (`prj/bin/Debug/conflict-converge-test.exe`) afterward and confirm both soak
trajectories (`Easy-vs-Medium`, `Medium-vs-Hard`) are unchanged — this fix should not alter combat
outcomes in the common case (units without a conflicting order were never affected), only close
the gap for units that had one.

---

## Bug 2: player auto-retreat's "nearest base" fallback measures from the wrong point

### Root cause
`src/game/private/core/Game.cpp:1469-1496`:

```cpp
// QoL player auto-retreat: opted-in units below threshold fall back
// to the rally point (or the nearest owned Base when no rally was
// ever placed). Shared RetreatIfLowHP with the AI's RetreatTick.
{
    Vector2 home = rallyPos;
    if (home.x == 0.0f && home.y == 0.0f)
    {
        float bestDistSq = -1.0f;
        registry.Each<Building>([&](Entity, const Building &building) {
            if (building.teamID != 0 || building.type != BuildingType::Base ||
                building.state != BuildingState::Operational)
            {
                return;
            }
            const cc::Vec2 corner = cc::TileToWorld(building.tileX, building.tileY);
            const Vector2 center = { corner.x + 32.0f, corner.y + 32.0f };
            const float dx = center.x - static_cast<float>(map.Width()) * 32.0f;
            const float dy = center.y - static_cast<float>(map.Height()) * 32.0f;
            const float distSq = dx * dx + dy * dy;
            if (bestDistSq < 0.0f || distSq < bestDistSq)
            {
                bestDistSq = distSq;
                home = center;
            }
        });
    }
    RetreatIfLowHP(registry, map, &occ, home, 0, kRetreatHealthFraction, true);
}
```

The intent (per the comment) is "the nearest owned Base." What it actually computes is the Base
**closest to the map's center point** — `(map.Width() * 32, map.Height() * 32)` is exactly
`(map pixel width / 2, map pixel height / 2)` since `cc::TILE_SIZE == 64.0f` (the `32.0f` here is
`TILE_SIZE / 2`, not a bug in the arithmetic itself, just an unexplained magic number standing in
for "the map's center"). That reference point has nothing to do with any retreating unit's actual
position.

With exactly one player Base, the loop trivially "picks" the only candidate regardless of the
reference point, so the bug is invisible — which is exactly why it shipped and why no existing
test (`retreat_tests.cpp`, `skirmish_tests.cpp`) catches it: none of them construct more than one
player-owned `BuildingType::Base`. With two or more Bases, this will deterministically pick
whichever Base happens to sit closer to the map's midpoint, unrelated to which Base is actually
closest to any unit that needs to retreat.

### Fix
`RetreatIfLowHP` (`src/game/public/units/Unit.h:265-266`) takes a single shared `Vector2 home` for
every retreating unit this frame — this signature is also used by `AICommander::RetreatTick`
(`AICommander.cpp:371`) and was deliberately kept as a "byte-for-byte equivalent refactor" of the
pre-existing AI logic (see `plans/QoL_Unit_Automation_Plan.md` §3). **Do not change
`RetreatIfLowHP`'s signature** — that would touch the AI path this plan's audit already confirmed
is correct and soak-neutral, for no benefit (the AI's `home` always comes from its own
commander's fixed `homeTile_`, which has never been buggy).

Fix only the "resolve `home` when no rally point is set" computation in `Game.cpp`, by changing
the reference point from the map's center to something that actually represents where the
player's units are: **the centroid of the player's own currently-alive units** (team 0). This
keeps the fix scoped to the single `home` value Game.cpp already computes once per frame (no
architectural change), while fixing the actual semantic bug — "nearest to our army" is a
reasonable, still-simple stand-in for "nearest to whichever unit is retreating" without requiring
`RetreatIfLowHP` to become per-unit-aware.

```cpp
Vector2 home = rallyPos;
if (home.x == 0.0f && home.y == 0.0f)
{
    // Reference point for "nearest owned Base": the centroid of the
    // player's own units, not the map's center (see
    // plans/Bugfix_Order_Flags_And_Retreat_Fallback_Plan.md -- measuring
    // from the map's center picked whichever Base happened to sit closest
    // to the map's midpoint, unrelated to where the player's army was).
    Vector2 armyCentroid = { 0.0f, 0.0f };
    int aliveCount = 0;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == 0 && unit.health > 0.0f)
        {
            armyCentroid.x += unit.position.x;
            armyCentroid.y += unit.position.y;
            ++aliveCount;
        }
    });
    if (aliveCount > 0)
    {
        armyCentroid.x /= static_cast<float>(aliveCount);
        armyCentroid.y /= static_cast<float>(aliveCount);
    }

    float bestDistSq = -1.0f;
    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID != 0 || building.type != BuildingType::Base ||
            building.state != BuildingState::Operational)
        {
            return;
        }
        const cc::Vec2 corner = cc::TileToWorld(building.tileX, building.tileY);
        const Vector2 center = { corner.x + 32.0f, corner.y + 32.0f };
        const float dx = center.x - armyCentroid.x;
        const float dy = center.y - armyCentroid.y;
        const float distSq = dx * dx + dy * dy;
        if (bestDistSq < 0.0f || distSq < bestDistSq)
        {
            bestDistSq = distSq;
            home = center;
        }
    });
}
```

If `aliveCount == 0` (no living team-0 units at all — an edge case that can only matter for a
single frame right before a loss condition triggers), `armyCentroid` stays `{0,0}`, which
degrades gracefully to the *old* (buggy but harmless-in-that-edge-case) behavior of picking
whichever Base is nearest the map's corner — acceptable since there's nothing left to retreat
anyway.

**More thorough alternative (optional follow-up, not required to close this bug):** make
`RetreatIfLowHP` resolve its own per-unit nearest home by accepting a list of candidate home
points (e.g. `const std::vector<Vector2> &homes`) instead of a single `Vector2 home`, and picking
the nearest one to each unit inside its own loop. This is strictly more correct (a real two-base
army retreats each unit to *its own* nearest base, not everyone to one shared point) but touches
the AI call site's signature too and would need its own soak-neutrality re-verification. Flag this
as a possible future refinement in the PR description; don't attempt it as part of this bug fix
unless asked.

### Tests to add
Extend `tests/unit-tests/retreat_tests.cpp` with a genuine multi-base case (the exact scenario
missing today, per the audit): construct two team-0 `Building`s of type `Base` at different tile
positions, place a damaged, `autoRetreat`-opted-in unit near one of them (not equidistant from the
map's center — pick positions where the "nearest to map center" and "nearest to the unit" answers
disagree, to make the old bug fail this test), leave `rallyPos` at `{0,0}` (unset), and assert the
resulting retreat destination is the Base nearest the unit's own position (or, with the recommended
army-centroid fix, nearest the centroid of the player's alive units — construct the test with a
single retreating unit and no other units so the centroid equals that unit's own position,
making the assertion unambiguous). Also keep the existing single-base test passing unchanged (it's
the case that was accidentally correct before).

Run the full suite afterward and confirm both soak trajectories are unchanged — every existing
skirmish/soak scenario has exactly one player Base, so this fix should be a no-op for all of them.
