# Plan: Area Repair (drag a zone, repair everything damaged inside it)

## Implementation status: SHIPPED (2026-09-17, commit `42c3556`)

- As designed (option b, left-drag sibling of area-build):
  `BuildingFootprintRect` + `QueryBuildingsInRect` (single-target scan
  deduped onto the helper), pure `CollectAreaRepairCandidates` +
  `AssignAreaRepair` in `Unit.cpp`, Shift-queueing through the existing
  `Repair` order path, drag preview with candidate markers,
  right-click/Esc cancel (cancel chain keeps `else if (altDown)`
  attached, so Alt+line formation can't arm mid-cancel).
- Integration notes: toggle key is **E** (verified free — WASD/camera
  and all bound letters claimed); the toggle is a Tier-1 remappable
  action (`AreaRepairMode` in the HotkeyMap table, bumping it 26 → 27).
- Tests: helper rect/query cases + assignment rules (§6) in
  `building_tests` / `area_repair_tests`. No deviations.

The last unshipped item from `plans/QoL_Orders_And_Queueing_Plan.md` §3 — single-target repair and
area-build both shipped; only "drag a rectangle, every selected Engineer repairs the damaged
units/buildings inside it" is still missing. Reclaim remains permanently deferred per the
roadmap's P9 (no wreck/salvage system) — not part of this plan.

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) before starting. This is the most involved of the
5 remaining items — it needs one genuinely new query function, a new input-mode gesture (all
right-drag modifiers are now claimed, see below), and a small assignment algorithm with no direct
precedent in this codebase. Budget accordingly; it's not a one-line change like the other four
remaining plans.

---

## 1. Single-target repair (shipped) — the behavior to generalize

- `IssueRepairOrder(Unit&, Entity)` (`Unit.h:249`, impl `Unit.cpp:168-178`) — no-ops unless the
  issuer is an Engineer; else `StopMoving` + `ClearOrders` + sets `hasRepairOrder`/`repairTarget`.
- Validation core, `RepairAim` (`Unit.cpp:301-334`), also exposed read-only as
  `CanRepairTarget(const Registry&, const Unit&, Entity)` (`Unit.h:254`, impl `Unit.cpp:336-340`).
  Rejects: non-Engineer issuer; for a `Unit` target — dead, foreign team, not `IsRepairableUnit`
  (`Unit.cpp:292-296`: mechanical types only — IFV/Artillery/LightTank/HeavyTank), or already full
  health; for a `Building` target — not `BuildingState::Operational`, foreign team, or already at
  `maxHealth`.
- Queued-order plumbing already generalizes to any target: `QueuedOrderKind::Repair` (`Unit.h:89`),
  `QueuedOrder{kind, pointA, pointB, target}` (`Unit.h:94-99`), dispatched via
  `DispatchQueuedOrder`'s `case QueuedOrderKind::Repair: IssueRepairOrder(unit, order.target);`
  (`Unit.cpp:217-219`), routed through `IssueOrEnqueue` (`Unit.cpp:261-277`) which queues behind
  the current order when Shift is held. **Area repair should issue through this exact same
  `IssueOrEnqueue`/`QueuedOrderKind::Repair` path, per-Engineer, per-target** — it already does
  everything needed (validation, queueing, dispatch); area repair only needs to *find and assign*
  the targets, not build a new order mechanism.
- Player gesture (single-target): `Game.cpp`'s `dispatchRightClickOrders` lambda (defined
  `Game.cpp:1083`), inside the `squad.size() == 1` branch:
  - `Game.cpp:1121`: gated on `ordered->type == UnitType::Engineer` (only fires for a *lone*
    selected Engineer).
  - `Game.cpp:1122-1144`: target identification — `PickUnitAt` + `CanRepairTarget` for a unit
    target; else scans `registry.Each<Building>` for one whose footprint tile-rect contains the
    clicked tile and passes `CanRepairTarget`, for a building target. One specific clicked
    target, never an area.
  - `Game.cpp:1145-1151`: `IssueOrEnqueue(..., input.ShiftDown(), QueuedOrder{Repair, {}, {},
    patient})`.

## 2. Area-build (shipped) — the drag-mode pattern to mirror

- State on `Game`: `std::optional<BuildingType> placingType;`, `bool placeDragActive = false;`,
  `Vector2 placeDragStart = {};` (`Game.h:149-151`).
- Toggled by `KEY_Z` (`Game.cpp:450-466`); left-click **press** starts the drag when
  `placingType.has_value()` (`Game.cpp:946-954`, pre-empting the normal box-select branch); release
  resolves it (`Game.cpp:1005-1046`):
  - World rect via the shared helper: `DraggedWorldBox(camera, placeDragStart,
    input.MouseScreen())` (`Selection.h:29`, impl `Selection.cpp:57-60`) — **reuse this directly**,
    don't reinvent a screen-to-world rect conversion.
  - Same 6px click-vs-drag threshold as ordinary box-select.
  - Drag case tiles candidate anchors across the tile range via `AreaBuildSlots(type, minTile,
    maxTile)` (`Building.h:74-79`, impl `Building.cpp:101-118`), stepped by `Footprint(type)` so
    placements never overlap, then loops the returned slots calling `PlaceBuilding` per slot,
    silently skipping ones that fail.
  - `CanPlaceBuilding` (`Building.h:72-73`, impl `Building.cpp:77-99`) is a side-effect-free
    predicate factored out specifically so the ghost preview can call it without side effects —
    **area repair should factor out an equivalent predicate** for the same reason (its own preview
    overlay, described below, needs to check validity without issuing anything).
  - Cancel: right-click or Escape while placing resets the mode with no order fired
    (`Game.cpp:1066-1070`, `:547`).

## 3. Gap: `QueryBuildingsInRect` does not exist

`QueryUnitsInRect(Registry&, Rectangle area, int teamID, std::vector<Entity>&)` exists
(`Combat.h:65`, impl `Combat.cpp:102-118`) — filters dead units, filters by `teamID` (or all teams
if negative), and keeps units whose 32x32 `HitboxOf(unit)` rect overlaps the query rect via
`CheckCollisionRecs`. **No building equivalent exists anywhere** — confirmed by grepping the whole
`src/game` tree; the only hits for "QueryBuildingsInRect" are this plan's own predecessor doc and
`missing_features.md` itself, both proposing it, never implementing it.

Add it fresh, in `Combat.h`/`.cpp` (next to `QueryUnitsInRect`, same file, for discoverability) or
`Building.h`/`.cpp` (co-located with `Footprint`/`CanPlaceBuilding`) — either is defensible; prefer
`Building.h`/`.cpp` since it needs `Footprint(BuildingType)` for the overlap test and that header
already owns the building-geometry helpers:
```cpp
// Buildings occupy a footprint rect (1x1-2x2 tiles, not a point), so this
// is a rect-vs-rect overlap test, not QueryUnitsInRect's point/hitbox test.
void QueryBuildingsInRect(Registry &registry, Rectangle worldArea, int teamID,
                          std::vector<Entity> &out);
```
Implementation: `registry.Each<Building>`, filter `teamID` (or all if negative, matching
`QueryUnitsInRect`'s convention), build each building's world-space footprint rect from
`tileX`/`tileY` + `Footprint(building.type)` (there's no existing helper for "building footprint as
a `Rectangle`" either — the single-target repair code inlines this tile-math itself at
`Game.cpp:1130-1143`; write a small shared `Rectangle BuildingFootprintRect(const Building&)` while
you're at it so both the old inline code and this new query share one implementation, rather than
adding a third copy of the same tile-to-rect math), then `CheckCollisionRecs` against
`worldArea`.

## 4. Gap: no "get selected Engineers" helper, no nearest-unclaimed-assignment precedent

- Every multi-unit gesture (formation move `Game.cpp:1105-1111`, line-formation
  `Game.cpp:1236-1242`, attack-ground `Game.cpp:1092-1098`) re-inlines the same
  `registry.Each<Unit>` + `isSelected` scan. Area repair needs the same idiom with an added
  `unit.type == UnitType::Engineer` filter — write it inline at the new call site, matching the
  existing style; don't invent a shared helper for a filter this simple unless a third caller
  wants the same "selected Engineers" list later.
- **No greedy nearest-unclaimed-target assignment exists anywhere.** The `bestDistSq`
  nearest-candidate scan pattern is used repeatedly (`ResolveGroundAttack`, `Combat.cpp:63-83`;
  the auto-retreat nearest-base fallback, `AICommander.cpp:403-421`; single-target acquisition,
  `Targeting.cpp:39,69-73` and `:102,119-122`), but always for one seeker picking one candidate —
  none of them guard against re-picking an already-claimed candidate across multiple iterations.
  Area repair's "N Engineers, M damaged targets, no double-assignment" needs a fresh loop
  composing this same `bestDistSq` idiom against a *mutable* candidate pool.

## 5. Gap: every right-drag modifier is already claimed — needs a new mode-toggle, not a new drag modifier

Current right-button gesture space, in full (`Game.cpp:1047-1251`):
- **Plain right-drag** (no modifier): camera pan if `menu.settings.rightDragPan` is on (accumulates
  `rightDragDist` past a 6px threshold via `MouseDeltaScreen()`); otherwise a plain right-click
  dispatches move/attack-move/single-repair/formation immediately.
- **Alt+right-drag**: line formation (`altDown` computed at `Game.cpp:1047`; comment there notes
  "Ctrl is groups, Shift is queueing — Alt stays unambiguous").
- **`X`-key toggle** (`attackGroundMode`): arms a one-shot; the *next* right-click, with any/no
  modifier, fires a ground-attack order instead of a move order.
- Ctrl and Shift are reserved on right-click's sibling gestures (Ctrl = control groups, Shift =
  order-queueing), so neither is available for a new right-drag modifier either.

**There is no unclaimed slot on right-drag.** Two options:
- **(a) A new mode-toggle key, consuming right-drag** (mirrors `X`/`attackGroundMode`'s
  "toggle-then-consume" shape, but consuming a *drag* instead of a single click): a new
  `bool areaRepairMode` toggled by an unclaimed key, and while active, right-drag resolves to area
  repair instead of the normal move/pan/line-formation logic — meaning it must be checked *before*
  those branches in the existing dispatch chain, the same way `placingType.has_value()` is checked
  first in the left-button press handler.
- **(b) A new mode-toggle key, consuming left-drag** (mirrors `placingType`/area-build's shape
  exactly — same button, different mode flag): a new `bool areaRepairMode` toggled by an unclaimed
  key, with its own `repairDragActive`/`repairDragStart` state (don't reuse `placeDragActive`, a
  second concurrent "what does left-drag mean right now" flag needs its own storage, exactly as
  `placingType` has its own rather than reusing `dragging`), checked in the same left-press guard
  chain as `placingType.has_value()`, ahead of the plain box-select fallthrough.

**Recommend (b)** — area repair is conceptually a sibling of area-build (both are "toggle a mode,
then drag a zone, then the mode auto-resolves or stays sticky for repeat use"), and area-build
already established this exact left-drag-under-mode-toggle shape. Reusing the same shape (rather
than teaching right-drag a 4th behavior) keeps the two "area command" features consistent with
each other for a player learning the game, and avoids adding yet another conditional branch to the
already-dense right-button dispatch chain in `Game.cpp:1047-1251`.

**Key choice**: every letter A/H/G/V/R/C/F/B/Z/X/T/J and P is already bound, plus F1/F5-F9/Escape/
Space/arrows/digits/Alt. **Grep the live `BindShortcuts()` body and the direct-poll sites
(control-group digits, area-build type-picker digits, map-editor palette digits) for the complete,
current claimed-key list before picking one** — this plan's research found the list above but more
may have been added since. No mnemonic letter is obviously free; pick pragmatically (e.g. a key not
already claimed) and note the choice in the PR description rather than assuming any specific one
is still open by the time this is implemented.

## 6. Who gets repaired if there are more Engineers than damaged targets (or vice versa)?
Decide explicitly, don't leave it implicit:
- **Fewer damaged targets than Engineers**: once every damaged target in the drag rect has an
  assigned Engineer, remaining selected Engineers get no new order (keep doing whatever they were
  doing) — don't force idle Engineers onto a repair order that duplicates another's target.
- **Fewer Engineers than damaged targets**: every Engineer gets assigned to its nearest *not yet
  claimed* target; leftover damaged targets in the rect simply don't get repaired this pass — no
  queueing of "come back for more" behavior needed, the player can drag again once the assigned
  repairs finish (repair orders queue via Shift exactly like single-target repair already does, so
  a second area-repair drag with Shift held would queue further work behind the first).
- **Unit vs. building priority**: no priority needed — treat both as one combined candidate pool
  for the nearest-unclaimed assignment (a `std::vector<Entity>` mixing unit and building entities
  is fine since `IssueOrEnqueue`'s `QueuedOrder::target` is already a plain `Entity`, and
  `IssueRepairOrder`/`CanRepairTarget` already dispatch correctly on whether the entity has a
  `Unit` or `Building` component).

---

## Implementation outline

1. `Building.h`/`.cpp`: add `Rectangle BuildingFootprintRect(const Building&)` (shared tile-to-rect
   helper) and `QueryBuildingsInRect(Registry&, Rectangle, int teamID, std::vector<Entity>&)` (§3).
2. `Game.h`: add `bool areaRepairMode = false;`, `bool repairDragActive = false;`,
   `Vector2 repairDragStart = {};` (§5, option b).
3. Bind a new key to toggle `areaRepairMode` (mirroring `KEY_Z`'s `BindShortcuts()` entry, §5).
4. `Game.cpp`'s left-press handler: add an `areaRepairMode` branch parallel to the existing
   `placingType.has_value()` branch, arming `repairDragActive`/`repairDragStart` instead of the
   normal box-select `dragging`.
5. `Game.cpp`'s left-release handler: add the `repairDragActive` resolution branch parallel to the
   `placeDragActive` one — compute the world rect via `DraggedWorldBox` (§2), collect selected
   Engineers (§4), collect damaged candidates via `QueryUnitsInRect` (filtered further to
   `IsRepairableUnit` + `health < BaseStats(type).health`, done client-side rather than extending
   the shared `QueryUnitsInRect` utility's signature) plus the new `QueryBuildingsInRect` (filtered
   to `state == Operational` + `health < maxHealth`), run the greedy nearest-unclaimed assignment
   (§4, §6), and call `IssueOrEnqueue(..., input.ShiftDown(), QueuedOrder{Repair, {}, {},
   assignedTarget})` per assigned Engineer.
6. Live preview while dragging (optional but recommended, mirrors area-build's ghost preview):
   while `repairDragActive`, draw the live drag rectangle (screen-space, same as the existing
   drag-box/line-formation previews) and optionally highlight in-rect damaged targets so the
   player can see what will be repaired before releasing.
7. Cancel: right-click or Escape while `areaRepairMode` is on resets it with no order, mirroring
   area-build's cancel handling.

---

## Tests

- `Building.h`/`.cpp` new helpers: `building_tests.cpp` — `BuildingFootprintRect` for 1x1 and 2x2
  types; `QueryBuildingsInRect` with buildings inside/outside/partially-overlapping a rect, filtered
  by team, mirroring `QueryUnitsInRect`'s existing test shape if one exists (check
  `combat_tests.cpp` for it).
- New area-repair dispatch/assignment logic: a new `area_repair_tests.cpp` (or a block in
  `order_queue_tests.cpp`) — construct several damaged units and buildings both inside and outside
  a drag rect, several selected Engineers, call the assignment function directly (factor the
  greedy-assignment loop into a testable pure function taking the Engineer list and candidate list,
  rather than burying it inline in `Game.cpp`'s input-handling code, so it's unit-testable without
  driving mouse input), and assert: each Engineer gets the nearest unclaimed target, no two
  Engineers get the same target, targets outside the rect are never assigned, and excess
  Engineers/targets are left over correctly per §6's rules.
