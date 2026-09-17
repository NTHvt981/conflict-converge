# Plan: Production & Economy UX QoL

## Implementation status: FULLY SHIPPED (updated 2026-09-17)

- Repeat production (`7f0f53b`), select-idle workers/army (`30d71d9`),
  select-all-of-type + select-all-factories (`deefb85`, incl. the minimal
  `Building::isSelected` groundwork future per-building UI can build on).

Covers, from `missing_features.md` § "Production & economy UX":
- Continuous / repeat production queue (build forever until cancelled)
- Select idle workers / idle army alert button
- Select-all-of-type and select-all-production hotkeys

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) **P4** (no building selection concept) before
starting the third item — it's the one with a real scoping decision to make.

## 0. Economy/production architecture recap (grounding)

- `ResourceSystem` (`src/game/public/economy/ResourceSystem.h`): plain `iron`/`oil` counters,
  `TrySpend` is atomic all-or-nothing, `TickIncome` with fractional carry.
- `ProductionQueue` (`src/game/public/economy/Production.h:19-42`,
  `src/game/private/economy/Production.cpp`): a **single FIFO** of `Item{UnitType, progress,
  buildTime}`. `Enqueue` charges upfront via `TrySpend`. `Update` advances only the front item and
  spawns via `UnitFactory::SpawnPrepaid` on completion.
- **There is no per-Factory-building queue** — the player has exactly one `ProductionQueue` member
  on `Game` (`Game.h:76`) shared across however many Factory buildings they own; likewise
  `AICommander` owns one queue per commander. `hasFactory`/`HasFactory()` just gates whether *any*
  Operational Factory exists — production doesn't care *which* one. This matters directly for
  "select-all-production" below.
- Player production trigger: `Hud.cpp:128-175`'s `DrawProductionPanel`, one `GuiButton` per
  `UnitType` from `ProductionMenuOrder()`, calling `queue.Enqueue(resources, order[clicked])` on
  click. No hotkey path exists today, only mouse clicks.
- `AICommander::MaintainProduction` (`AICommander.cpp:273-296`) already keeps its queue
  perpetually topped up to a target unit count — this is the closest existing analogue to "repeat
  production," just reimplemented as a per-tick decision loop rather than a persisted queue flag.

---

## 1. Continuous / repeat production queue

### Design: per-item `repeat` flag (not a queue-level repeat mode)
Add to the private `Item` struct in `Production.h:35-40`:
```cpp
struct Item
{
    UnitType type = UnitType::Infantry;
    float progress = 0.0f;
    float buildTime = 1.0f;
    bool repeat = false; // QoL: re-charge and restart instead of being popped on completion
};
```
Add an `Enqueue` overload (default `repeat=false` preserves every existing call site
unmodified — `AICommander.cpp`, `Skirmish.cpp`, `Hud.cpp`):
```cpp
bool Enqueue(ResourceSystem &resources, UnitType type, bool repeat = false);
```

### `Update` behavior change
In `ProductionQueue::Update` (`Production.cpp:40-65`), where the finished front item is currently
erased (`items_.erase(items_.begin())`), branch on `repeat`:
```cpp
if (done.repeat)
{
    if (resources.TrySpend(CostOf(done.type).iron, CostOf(done.type).oil))
    {
        items_.front().progress = 0.0f; // re-arm in place, keep FIFO order
        // (don't erase; don't re-push to the back — this keeps a repeating
        // item's position in the queue stable rather than cycling it to the
        // end, which would let other queued items starve it of turns)
    }
    else
    {
        // Insufficient funds: pause, don't drop the repeat flag — mirrors
        // AICommander::MaintainProduction's "insufficient funds: income
        // will retry next tick" idiom (AICommander.cpp:289-291). Erase is
        // wrong here (would silently disable repeat); leave the item in
        // place with progress==buildTime so the next Update call retries
        // the TrySpend without re-running the build timer.
    }
}
else
{
    items_.erase(items_.begin()); // unchanged existing behavior
}
```
Handle the "insufficient funds, stay parked at 100% progress" case carefully so `Update` doesn't
re-trigger the completion branch repeatedly with side effects (e.g. don't call `SpawnPrepaid`
again) — guard so a parked-at-completion repeating item only attempts one `TrySpend` per `Update`
call and does nothing else until it succeeds.

### Cancel semantics
`CancelTop(resources)` (`Production.cpp:28-38`) already fully refunds and removes the front
item unconditionally — this already gives "until cancelled" for free, no changes needed there.
Consider whether cancelling a `repeat` item mid-build should refund the in-progress build's cost
(current `CancelTop` behavior for any item) — yes, keep consistent, no special-case needed.

### UI
`Hud.cpp:128-175`'s `DrawProductionPanel` needs a per-unit-type repeat toggle. Add a small toggle
control (checkbox or right-click-to-toggle) next to each of the 7 `GuiButton`s in the loop
(`Hud.cpp:142-162`), calling a new `queue.SetRepeatNext(UnitType, bool)` that affects the *next*
`Enqueue` call for that type — or simpler: make the repeat toggle apply directly by having the
button click pass `repeat = repeatToggled[type]` into `Enqueue`. Either is fine; keep the state
(`std::array<bool, unitTypeCount> repeatToggled` or similar) on whatever object owns
`DrawProductionPanel`'s call site (`Game` or `Hud`, match existing state-ownership convention —
`Hud.cpp` functions appear to be mostly stateless pure builders per the codebase's draw-call
convention, so `Game` is the more consistent home).

### Tests
`production_tests.cpp`'s existing `Rig` harness (registry/resources/dispatcher/factory/spawn
counter) is the exact fixture to extend: enqueue with `repeat=true`, run `Update` past `buildTime`
twice in a row, assert `queue.Size() == 1` after each completion (never drops to 0) and
`resources` charged twice, spawn counter incremented twice. Add a case where funds run out after
the first cycle: assert the item stays parked (size still 1, no third spawn) until funds are
replenished, then completes on the next `Update` once affordable.

---

## 2. Select idle workers / idle army alert button

### "Idle" is already derivable — no new state needed
`Unit::state == UnitState::Idle` combined with `!hasMoveOrder && !hasPath && !hasRepairOrder`
unambiguously means "nothing to do." Confirmed via `UpdateUnit`'s no-target fallthrough branch in
`Unit.cpp`, which explicitly sets `state = UnitState::Idle` when there's truly nothing pending.

### Critical gotcha, verified in source: a channeling repair-Engineer also reports `Idle`
Inside `Unit.cpp`'s repair branch, the in-range/actively-repairing case sets
`unit->state = UnitState::Idle` too (this is presumably a pre-existing minor mislabeling — the
`state` enum wasn't extended with a `Repairing` case). **A "select idle" query must additionally
exclude `hasRepairOrder`**, or it will falsely flag busy repair-Engineers as idle. Do not skip this
check — it was confirmed by direct source inspection, not inferred.

### Worker vs. army distinction
No separate `Harvester`/`Worker` type exists — `UnitType::Engineer` is the sole economic-gathering
unit, confirmed by `Nodes.cpp`'s `GatherTick` (gated on `type == Engineer`) and
`AICommander::HarvesterCount()`/`CombatUnitCount()`'s exact `type == Engineer` / `type !=
Engineer` split. **Reuse this exact split** — don't invent a new "IsWorker" concept, just filter on
`UnitType::Engineer` directly, matching the codebase's existing convention.

### New query function
Add to `Selection.h/.cpp`:
```cpp
// Selects every idle (state==Idle, no move/path/repair order) same-team
// unit, optionally restricted to Engineers only (workersOnly=true) or
// excluding them (workersOnly=false selects idle army — i.e. everyone else).
// Note: workersOnly is a 3-way-ish choice in practice — see call sites below
// for how "idle workers" vs "idle army" each set it.
int SelectIdle(Registry &registry, int teamID, bool workersOnly);
```
Implementation: `registry.Each<Unit>` filter `teamID == teamID && state == UnitState::Idle &&
!hasMoveOrder && !hasPath && !hasRepairOrder && (workersOnly ? type == UnitType::Engineer : type
!= UnitType::Engineer)`, set `isSelected` (replace, not add — matches `SelectOnly`'s exclusive
semantics), return count.

### "Idle army alert button" — camera-jump half is out of scope here
The "alert" half (camera pan/jump to the idle units) depends on camera-control primitives covered
in [QoL_Camera_Minimap_Info_Plan.md](QoL_Camera_Minimap_Info_Plan.md) (specifically the
`GameCamera::Pan`/jump-to-point mechanism already used for minimap click-to-move). This plan
implements the **selection** half (`SelectIdle`) plus a `Hud.cpp` button wired to call it; if no
idle units exist, the button should be visibly disabled/greyed (compute the count first via
`SelectIdle`'s dry-run — or add a non-mutating `CountIdle(registry, teamID, workersOnly)` sibling
so the button can show a live count without side effects on every frame it's just rendering, not
being clicked).

### Tests
New block in `selection_tests.cpp`: construct units in various states (moving, repairing,
genuinely idle, both Engineer and non-Engineer types), call `SelectIdle` with both
`workersOnly` values, assert exactly the expected subset gets selected — **explicitly include a
repairing Engineer in the fixture and assert it is NOT selected as idle**, since this is the
gotcha most likely to regress silently if missed.

---

## 3. Select-all-of-type and select-all-production hotkeys

### Select-all-of-type
Straightforward addition to `Selection.h/.cpp`, mirroring `SelectInRect`'s shape:
```cpp
int SelectAllOfType(Registry &registry, UnitType type, int teamID, bool add);
```
Trigger: read the currently-selected unit's type via `SelectedUnit(registry)` +
`registry.Get<Unit>`, call `SelectAllOfType(registry, selectedType, myTeamID, /*add=*/false)`.
Bind under a new key in `Game::BindShortcuts` (`Game.cpp:192+`) — **grep the full body of
`BindShortcuts` first to enumerate every already-claimed key** before picking a new binding (known
claimed so far: F1, P, F5-F9 [+Shift chords], A, H, G, V, R, Escape, Space — there may be more;
don't assume this list is exhaustive without checking the live file).

**`ShortcutRegistry` constraint**: only a plain binding + one Shift-chord per key
(`Shortcuts.h:35-41`) — no Ctrl slot (see roadmap P1). A plain, distinct key is sufficient for this
feature (no modifier needed), so no `ShortcutRegistry` changes required here.

### Select-all-production — needs an explicit scoping decision
**Buildings have no selection concept at all today** (no `Building::isSelected`, no
`PickBuildingAt`), and there is **no multi-factory-aware production system** to select between
(one global `ProductionQueue` per player, not per-Factory-building — see §0). Three possible
readings, evaluated:
1. *"Select all Factory buildings"* — requires inventing building selection from scratch (new
   `Building::isSelected`, new `SelectAllBuildings(Registry&, BuildingType, int teamID)`, and a new
   HUD panel since `DrawSelectionPanel` only ever reads `SelectedUnit`). Real but substantial scope
   for what should be a small QoL hotkey.
2. *"Select all units currently in the production queue"* — doesn't make sense mechanically:
   queued items aren't entities yet (`ProductionQueue::Item` has no `Entity` field; units are only
   created at `SpawnPrepaid` on completion). Discard this reading.
3. *"Jump to/focus the Factory so the production hotbar is visible"* — given there's only one
   global queue and `DrawProductionPanel` isn't tied to a specific building at all, this reduces to
   "make sure the production panel is on-screen/focused," which today it always is whenever
   `hasFactory` is true (`Hud.cpp:128`) — i.e., there's nothing to "select" under this reading
   either, since the panel isn't hideable/toggled off by anything currently.

**Recommendation: implement reading (1) minimally** — add `bool Building::isSelected` (new field,
following the exact same idiom as `Unit::isSelected`), a `SelectAllBuildings(Registry&,
BuildingType type, int teamID)` in `Selection.h/.cpp`, bind a key that calls
`SelectAllBuildings(registry, BuildingType::Factory, myTeamID)`, and extend `DrawSelectionPanel`
(`Hud.cpp:106-119`) to also show a simple building summary (count of selected factories, or just a
highlight ring drawn on selected buildings in the render loop, mirroring the unit selection-outline
draw at `Game.cpp:~999-1007`) — this is useful groundwork for future building-selection features
too (auto-repair toggle UI in
[QoL_Unit_Automation_Plan.md](QoL_Unit_Automation_Plan.md) §4 could later build on it), not just a
one-off hack for this hotkey. Keep the *use* of building selection minimal here (just a visual
highlight + a count), don't build a full building command UI as part of this feature.

### Tests
`selection_tests.cpp`: `SelectAllOfType` test mirroring `SelectInRect`'s existing test shape
(mixed types/teams, assert only matching same-team units selected, `add=false` replaces prior
selection). New `SelectAllBuildings` test in `building_tests.cpp` or a new fixture: mixed building
types/teams, assert only matching same-team buildings get `isSelected` set.
`shortcuts_tests.cpp`: verify the new key routes to the right handler using the existing
`Fire(key)`/`FireChord(key)` headless test entry points (routing only — test the selection logic
itself independently, decoupled from key binding, per the project's established testing
convention).
