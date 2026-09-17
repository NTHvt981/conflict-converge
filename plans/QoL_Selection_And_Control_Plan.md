# Plan: Selection & Control QoL

## Implementation status: PARTIAL (updated 2026-09-17)

- Shipped: input plumbing P1/P5/P6 (`f6d6392`), control groups + auto-add
  (`4a46968`), line formation (`18067b9`), slowest-speed toggle (`9e5e102`).
- NOT shipped: double-click select-type (§2) — `SelectAllOfType` (C key)
  covers the use case without the double-click gesture itself.

Covers, from `missing_features.md` § "Selection & control (highest demand)":
- Control groups (Ctrl+number assign, Shift adds, on-screen markers)
- Auto-add factory output to a control group
- Double-click selects all units of type on screen
- Line-draw formation placement (drag a line, units space along it)
- Move-at-slowest-speed toggle for mixed groups

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) first — this plan depends on prerequisites **P1**
(Ctrl modifier), **P5** (`RightDown()`), and **P6** (`DraggedWorldBox` helper) defined there.
Implement those three first; they're small and mechanical.

All file paths are relative to the repo root. Line numbers are as of the investigation that
produced this plan — re-grep before editing if the file has moved on.

---

## 0. Prerequisite work (do first, ~1-2 hours)

### 0.1 `InputManager::CtrlDown()`
File: `src/game/public/app/InputManager.h`, `src/game/private/app/InputManager.cpp`.
Mirror `shiftDown_`/`ShiftDown()` exactly:
- Header: add `bool CtrlDown() const;` near `ShiftDown()` (`InputManager.h:37`), and a private
  `bool ctrlDown_ = false;` near `shiftDown_` (`InputManager.h:47`).
- `Snapshot(...)`: add a `bool ctrlDown = false` trailing parameter (keep existing params/order,
  append at the end so existing call sites in `input_manager_tests.cpp` keep compiling), store into
  `ctrlDown_`.
- `PollLive()` (`InputManager.cpp:11-17`): pass
  `IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)` into the new `Snapshot` argument.
- Add a test block to `tests/unit-tests/input_manager_tests.cpp` mirroring the existing
  `ShiftDown()` test (search for `shiftDown` in that file for the exact shape to copy).

### 0.2 `InputManager::RightDown()`
Same file. Mirror `leftDown_`/`LeftDown()` (`InputManager.h:34,42-48`; `InputManager.cpp:11-28`):
- Add `bool RightDown() const;` + private `bool rightDown_ = false;`.
- Add `bool rightDown = false` trailing `Snapshot(...)` parameter, store it.
- `PollLive()`: pass `IsMouseButtonDown(MOUSE_BUTTON_RIGHT)`.
- Mirror the existing `leftDown_` test in `input_manager_tests.cpp` for `rightDown_`.

### 0.3 `Selection::DraggedWorldBox`
File: `src/game/public/app/Selection.h`, `src/game/private/app/Selection.cpp`.
Add next to `NormalizeRect` (`Selection.h:18`):
```cpp
// Converts a screen-space drag (start/end in screen pixels) into a
// normalized world-space rectangle, via the camera's current view.
Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenStart, Vector2 screenEnd);
```
Implementation: `NormalizeRect(camera.ScreenToWorld(screenStart), camera.ScreenToWorld(screenEnd))`
— this is exactly what `Game.cpp:631-635` currently does inline for box-select; replace that
inline code with a call to this new function once it exists (keeps behavior identical, adds a
seam every other drag-based feature reuses). Add a small unit test in a new or existing
`selection_tests.cpp` block asserting the round-trip matches `NormalizeRect` given identity/simple
camera transforms.

---

## 1. Control groups + auto-add factory output

### Design decision: store the group number on `Unit`, not a side-table
Follow the codebase's existing idiom (`Unit::isSelected` is a per-unit field, not a side
`SelectionSet` class — see `Unit.h:97`). Add:
```cpp
// QoL: control-group membership, -1 = none. Assigned via Ctrl+number,
// extended via Shift+number-recall (add current selection to group),
// recalled via plain number (select all members). Not saved (transient
// UX convenience, like isSelected) unless product wants persistence —
// default to NOT persisting in SaveGame.proto to avoid a schema bump;
// revisit if playtesting shows groups should survive save/load.
int controlGroup = -1;
```
Add this field to `Unit` (`src/game/public/units/Unit.h`), right after `isSelected` (`Unit.h:97`).
Do **not** add it to `proto/savegame.proto` / `SaveGame.cpp` in this pass — keep scope minimal;
note the decision in a comment so a future pass can revisit deliberately.

### Storage for "which numbers currently have members" (for the on-screen strip)
No new storage needed — derive it every frame by scanning, exactly like `squad` is built at
`Game.cpp:645-651`. Don't cache a `std::array<std::vector<Entity>, 10>` on `Game`; it would need
manual pruning on unit death and the codebase has no precedent for that pattern (every other
selection-adjacent query re-scans `registry.Each<Unit>` fresh). A per-frame scan of at most a few
hundred units is not a performance concern here (see the existing `perf: 200 units x 600 frames`
test in the suite for the actual budget).

### Input handling
This does **not** go through `ShortcutRegistry` — number-key groups need 3-way disambiguation
(assign / recall / add) that the registry's plain/Shift-chord model can't express in one binding.
Instead, poll directly inside `Game::Update()`'s existing gesture block (near `Game.cpp:584-667`,
after the drag-box resolution, before or after the right-click order block — order doesn't matter,
they're independent input paths):

```cpp
for (int key = KEY_ONE; key <= KEY_NINE; ++key)
{
    if (!IsKeyPressed(key)) continue;
    const int groupNum = key - KEY_ONE; // 0-8; reserve KEY_ZERO for group 9 if wanted
    if (input.CtrlDown())
    {
        // Assign: clear this group from everyone else, then stamp it on the
        // current selection (mirrors SelectOnly's exclusive-set idiom,
        // Selection.cpp:24-32, but keyed on controlGroup instead of isSelected).
        registry.Each<Unit>([&](Entity, Unit &u) {
            if (u.controlGroup == groupNum) u.controlGroup = -1;
        });
        registry.Each<Unit>([&](Entity, Unit &u) {
            if (u.isSelected) u.controlGroup = groupNum;
        });
    }
    else if (input.ShiftDown())
    {
        // Add current selection to the group, without clearing existing members.
        registry.Each<Unit>([&](Entity, Unit &u) {
            if (u.isSelected) u.controlGroup = groupNum;
        });
    }
    else
    {
        // Recall: select exactly this group's members (mirrors SelectOnly's
        // exclusive semantics, but membership-based instead of single-entity).
        registry.Each<Unit>([&](Entity, Unit &u) {
            u.isSelected = (u.controlGroup == groupNum);
        });
    }
}
```
Note: a unit can only belong to one group at a time in this design (assigning to group 3 doesn't
remove it from group 1 in the *Shift-add* branch — only the *Ctrl-assign* branch does an exclusive
clear, and only for the target group, not all groups). If product wants "a unit can be in multiple
groups simultaneously," `controlGroup` needs to become a bitmask (`int controlGroups` with bit N =
membership in group N) instead of a single int — flag this as a decision point in the PR
description; the plan above assumes single-group membership per unit (matches classic RTS
convention: assigning to a new group removes you from the group you're recalled from... actually
most RTS allow multi-group membership. **Recommend the bitmask from the start** to avoid a painful
migration later — trivial extra cost now: `unsigned int controlGroups = 0;`, test bit N via
`(controlGroups & (1u << n)) != 0`, assign via `|=`, and the exclusive-clear-on-Ctrl-assign should
probably NOT clear other groups' membership at all (Ctrl+3 puts the selection *into* group 3,
doesn't remove them from group 1) — simplifies the assign branch to just `u.controlGroups |= (1u
<< groupNum)` when selected, cleared for group N when `Ctrl` is held (replace, not add) vs. `|=`
under Shift (add). Pick the bitmask design; it's barely more code and avoids a rewrite.

### Extract new pure functions for testability
Following the roadmap's "pure logic vs draw calls" convention, put the group-membership logic in
`Selection.h/.cpp` as testable functions rather than inline in `Game.cpp`:
```cpp
// Selection.h
void AssignControlGroup(Registry &registry, int groupBit);   // Ctrl+N
void AddToControlGroup(Registry &registry, int groupBit);    // Shift+N
int  RecallControlGroup(Registry &registry, int groupBit);   // plain N; returns count selected
```
`Game.cpp`'s per-frame loop just dispatches to these based on `CtrlDown()`/`ShiftDown()`. Test them
in a new `tests/unit-tests/control_group_tests.cpp` (register in `tests/main.cpp` as
`RunControlGroupTests`), following `selection_tests.cpp`'s `SpawnAt`-helper pattern.

### Auto-add factory output to a group
1. Add a "default group for new production" concept. Simplest viable scope: a single
   `Game`-level `int autoAddGroupBit = -1;` (not per-factory — there's only one global
   `ProductionQueue` today per `Game.h:76`, see roadmap P4/plan-4 for why per-factory doesn't exist
   yet). A UI affordance (e.g. a small button/hotkey — reuse `Ctrl+Shift+N`? or just document as a
   settings-panel toggle) sets `autoAddGroupBit`.
2. `ProductionQueue::Update` (`src/game/public/economy/Production.h:28`,
   `src/game/private/economy/Production.cpp:40-65`) currently discards `SpawnPrepaid`'s return
   value at `Production.cpp:57`. Change `Update`'s signature to return the spawned `Entity`
   (`kInvalidEntity` when nothing completed this call):
   ```cpp
   Entity Update(UnitFactory &factory, int teamID, Vector2 rallyPos, float dt);
   ```
   Update the body to capture and return `factory.SpawnPrepaid(...)`'s result instead of
   discarding it. This is a signature change — grep all call sites (`Game.cpp:787`ish,
   `AICommander.cpp`, `Skirmish.cpp`, and `production_tests.cpp`) and update them to use or
   discard the return value as appropriate; AI/skirmish call sites can just ignore it (`(void)
   queue_.Update(...)` or leave the return unused, C++ allows ignoring a return value silently).
3. At the player's call site (`Game.cpp:787`ish, right after `queue.Update(...)`):
   ```cpp
   const Entity spawned = queue.Update(factory, teamID, rallyPos, dt);
   if (spawned != kInvalidEntity && autoAddGroupBit >= 0)
   {
       if (Unit *u = registry.Get<Unit>(spawned)) { u->controlGroups |= (1u << autoAddGroupBit); }
   }
   ```

### On-screen markers
Two parts, both pure `Game.cpp`/`Hud.cpp` draw-call additions (no new tests needed per the
project's "draw calls are untested" convention):
1. **Per-unit badge**: in the existing per-unit draw loop (`Game.cpp:~999-1007`, right where the
   selection outline is drawn), add a small `DrawText` showing the lowest set bit's group number
   when `unit.controlGroups != 0`, positioned near the unit's health bar.
2. **Bottom-of-screen group strip**: new `Hud.cpp` function `DrawControlGroupStrip(const Registry
   &registry, int myTeamID)` mirroring `DrawSelectionPanel`'s shape (`Hud.cpp:106-119`) — for each
   of the 10 possible groups, count members (`registry.Each<Unit>` scan), draw a numbered box
   (dimmed if empty, highlighted if it's the currently-selected group). Call from `Game.cpp`'s HUD
   section near the other `Draw*Panel` calls.

---

## 2. Double-click selects all units of type on screen

### Click-timer state
Add to `InputManager` (mirrors the "poll vs. snapshot" split already used for `leftPressed_` etc.,
`InputManager.cpp:11-28`):
```cpp
// InputManager.h, private:
double lastLeftClickTime_ = -1.0;
Vector2 lastLeftClickPos_ = {};
bool doubleClicked_ = false;
// public:
bool DoubleClicked() const;
```
In `PollLive()` (`InputManager.cpp:11-17`), when a fresh left-press is detected, compare
`GetTime()` (raylib) against `lastLeftClickTime_` and the click position against
`lastLeftClickPos_` (small pixel tolerance, e.g. 8px, to allow for hand tremor) — if within a
double-click window (e.g. 0.35s) and close enough, set `doubleClicked_ = true` for this frame only
(reset next frame like `leftPressed_`'s edge semantics), else just record the new time/pos as the
"first click" of a potential pair. To keep this testable without real timestamps, add a
`Snapshot(...)` overload/parameter that accepts an explicit `double nowSeconds` — mirror
`input_manager_tests.cpp`'s existing pattern of driving `Snapshot(...)` directly rather than
`PollLive()`.

### Selection logic
New function in `Selection.h/.cpp`:
```cpp
// Selects every same-team unit of `type` whose body intersects the current
// camera viewport. Mirrors SelectInRect's shape/semantics but filters by
// type+team instead of an explicit drag rect, and computes the rect from
// the camera's viewport instead of taking one as a parameter.
int SelectAllOfTypeOnScreen(Registry &registry, const GameCamera &camera,
                            int screenWidth, int screenHeight,
                            UnitType type, int teamID);
```
Implementation: build the viewport world-rect via `camera.ScreenToWorld({0,0})` and
`camera.ScreenToWorld({screenWidth, screenHeight})` (same conversion already used for drag boxes),
then iterate `registry.Each<Unit>` filtering `unit.type == type && unit.teamID == teamID` and
body-center-in-rect (reuse `SelectInRect`'s exact hitbox inset logic, `Selection.cpp:56-82`, for
consistency), setting `isSelected` (replacing prior selection, `add=false` semantics — double-click
should not require Shift to work, matching genre convention).

**Flag explicitly in the PR**: `PickUnitAt` has no team check today (`Selection.cpp:6-22`) — you
can currently left-click-select and even right-click-order enemy units. Decide whether
double-click should filter to the player's team only (recommended — hardcode `teamID = 0` at the
`Game.cpp` call site, matching however the player's team is identified elsewhere in `Game.cpp`) or
inherit the clicked unit's team (so double-clicking an enemy selects all visible enemies of that
type, harmless since you can't issue them orders anyway, but somewhat confusing UX) — recommend
the former.

### Wiring
In `Game.cpp`'s left-click resolution (`Game.cpp:610-627`, the "box is tiny → click" branch): after
`PickUnitAt` succeeds, check `input.DoubleClicked()`; if true, call
`SelectAllOfTypeOnScreen(registry, camera, GetScreenWidth(), GetScreenHeight(), hitUnit->type,
hitUnit->teamID)` instead of `SelectOnly`.

### Tests
`selection_tests.cpp`: new block constructing several units of mixed types both inside and outside
a fake viewport rect, asserting `SelectAllOfTypeOnScreen` selects only same-type-same-team-in-view
units. `input_manager_tests.cpp`: new block driving two `Snapshot()` calls with close timestamps/
positions asserting `DoubleClicked()` fires, and one with a far timestamp asserting it doesn't.

---

## 3. Line-draw formation placement

### Pure offset-generator function (clone of `FormationOffsetsFP`)
File: `src/game/public/units/Formation.h`, `src/game/private/units/Formation.cpp`. The existing
`FormationOffsetsFP(count, cellSize)` (`Formation.cpp:29-44`) is a pure `count → grid offsets`
function, already isolated and unit-tested independent of any `Registry` — clone its shape:
```cpp
// Evenly spaces `count` points along the segment lineStart->lineEnd
// (inclusive of both ends when count > 1; a single unit gets the midpoint).
// Pure geometry, unit-testable without a Registry (mirrors FormationOffsetsFP).
std::vector<cc::Vec2> LineFormationPositions(std::size_t count, cc::Vec2 lineStart, cc::Vec2 lineEnd);
```
Implementation: for `count == 0` return empty; `count == 1` return `{(lineStart+lineEnd)/2}`;
otherwise `lerp(lineStart, lineEnd, i / float(count - 1))` for `i` in `[0, count)` (glm's `mix` is
already available via `MathUtils.h`, used elsewhere in the codebase per the research notes).

### Orchestrator function (clone of `IssueFormationMoveFP`)
```cpp
// Orders `units` into a line between lineStart/lineEnd (world space),
// spaced by LineFormationPositions, each snapped to its nearest enterable
// tile exactly like IssueFormationMoveFP. Clone of that function's per-unit
// loop (Formation.cpp:63-101) with the offset source swapped.
void IssueLineFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                              const TileMap &map, const OccupancyGrid &occ,
                              Vector2 lineStartWorld, Vector2 lineEndWorld);
```
Body: get `LineFormationPositions(units.size(), ToGlm(lineStartWorld), ToGlm(lineEndWorld))`, then
for each unit: snap the position to a tile via `cc::WorldToTile`, sanitize via
`NearestEnterableTile` (exactly as `IssueFormationMoveFP` does at `Formation.cpp:94-96`), and call
`IssuePathOrderFootprint` (`Formation.cpp:97-99`). This is close enough to
`IssueFormationMoveFP`'s body that you may want to factor a shared per-unit-dispatch helper taking
a `std::vector<cc::IVec2> slots` computed upstream by either offset generator — your call based on
how much duplication bothers you; either is fine for a first cut.

### Gesture: right-drag with a modifier
Right-click today is a single-frame edge event that always issues an order immediately
(`Game.cpp:641-667`) — there is no existing right-drag concept. Add:
1. `Game` members: `bool rightDragging = false; Vector2 rightDragStart = {};` (mirrors
   `dragging`/`dragStart`, `Game.h:100-101`).
2. On `input.RightPressed()` **while holding a chosen modifier key** (recommend a dedicated key
   rather than overloading Ctrl/Shift, since Ctrl is reserved for control groups and Shift is
   reserved for order-queueing per plan 2 — e.g. `IsKeyDown(KEY_LEFT_ALT)`; confirm no existing
   binding claims it by grepping `BindShortcuts`, `Game.cpp:192-364`): set `rightDragging = true;
   rightDragStart = input.MouseScreen();` **instead of** issuing an order immediately.
3. Every frame while `rightDragging`, draw the live line preview (`DrawLineEx` from
   `rightDragStart` to `input.MouseScreen()`, screen-space — mirrors the existing drag-box preview
   at `Game.cpp:1050-1052`).
4. On release (`rightDragging && !input.RightDown()`): compute the world-space line via
   `DraggedWorldBox`... actually a line isn't a rect — add a small sibling helper
   `std::pair<Vector2, Vector2> DraggedWorldLine(const GameCamera&, Vector2 screenA, Vector2
   screenB)` (just two `ScreenToWorld` calls, no normalization needed) next to `DraggedWorldBox` in
   `Selection.h/.cpp`. Then call `IssueLineFormationMoveFP(registry, squad, map, occ, lineStart,
   lineEnd)` using the same `squad`-collection idiom as the existing right-click handler
   (`Game.cpp:645-651`), and reset `rightDragging = false`.
5. **Without the modifier held**, right-press/release behaves exactly as today (plain click issues
   the existing single-point/formation order) — this plan must not change existing right-click
   behavior when the modifier isn't held.

### Tests
New `tests/unit-tests/line_formation_tests.cpp` (`RunLineFormationTests`, registered in
`tests/main.cpp`): test `LineFormationPositions` for count 0/1/2/5 against hand-computed
expectations (mirror `formation_tests.cpp`'s `FormationOffsetsFP` test shape), and test
`IssueLineFormationMoveFP` end-to-end with a small squad + open map asserting distinct destination
tiles roughly along the line (mirror `formation_tests.cpp`'s `IssueFormationMoveFP` test).

---

## 4. Move-at-slowest-speed toggle for mixed-speed groups

### Data model
Add to `Unit` (`Unit.h`, near `speed`, `Unit.h:93`):
```cpp
// QoL: when >= 0, caps this unit's effective movement speed below its own
// Unit::speed for the current order (used for "move at slowest speed" group
// orders so fast units don't outrun slow ones). -1 = uncapped. Reset to -1
// on every new order and on arrival/cancel (see call sites below) so it
// never leaks into an unrelated later order.
float speedCapPixelsPerSec = -1.0f;
```

### Effective-speed helper
Add near `Unit`'s other small inline helpers (`Unit.h`, next to `SnapUnitToTile`):
```cpp
inline float EffectiveSpeed(const Unit &unit)
{
    return unit.speedCapPixelsPerSec >= 0.0f ? std::min(unit.speed, unit.speedCapPixelsPerSec)
                                              : unit.speed;
}
```

### Call-site changes
Every `UpdateUnitMovement(..., unit->speed, ...)` call site in `Unit.cpp`'s `UpdateUnit` driver
(three of them per the investigation: the repair-approach leg, the attack-move chase leg, and the
post-target-check drive leg — grep `UpdateUnitMovement(*unit` in `Unit.cpp` to find all current
call sites, since the exact line numbers may have shifted since this plan was written) must change
from `unit->speed` to `EffectiveSpeed(*unit)`.

### Set point
`formation::IssueFormationMoveFP` (`Formation.cpp:63-101`) and the new
`IssueLineFormationMoveFP` (§3 above) both gain a `bool slowestSpeed` parameter. When true, before
the per-unit loop, compute `float minSpeed = min over units[i]->speed` (skip missing/destroyed
units, same guard already used for offsets), then inside the loop set
`unit->speedCapPixelsPerSec = minSpeed;` right before calling `IssuePathOrderFootprint`.

Thread the toggle from `Game.cpp`'s right-click handler: add a `bool moveAtSlowestSpeed = false;`
member to `Game` (near `dragging`, `Game.h:99-103`), flip it via a new keybinding in
`BindShortcuts` (e.g. a toggle key — pick one not already claimed; grep `BindShortcuts` for the
full claimed-key list first), and pass it through at the `formation::IssueFormationMoveFP(...)`
call site (`Game.cpp:663-664`).

### Clear point
`speedCapPixelsPerSec` must reset to `-1.0f` wherever an order clears today, or a capped unit stays
capped on its *next*, unrelated order. Concretely add the reset line everywhere `hasMoveOrder`/
`hasPath` both get cleared:
- The `Arrive`/`CancelAtBlocked`-style helpers inside `Unit.cpp` (wherever `UpdateUnitMovement`
  clears them on arrival/cancel — grep for where `blockedTime`/`blockedRepaths` get reset, since
  those are cleared at exactly the same points per the movement-stall fix work; add
  `unit.speedCapPixelsPerSec = -1.0f;` alongside them).
- The `KEY_SPACE` halt handler (`Game.cpp:341-363`), alongside its existing
  `hasMoveOrder`/`hasPath`/`target`/`attackMove`/`hasRepairOrder` clears.
- Every `Issue*Order` function in `Unit.cpp` that's *not* a formation move (plain `IssueMoveOrder`,
  `IssuePatrolOrder`, `IssueRepairOrder`, `IssueAttackMoveOrder(Footprint)`) should also reset it to
  `-1.0f` at the top, since issuing any of these single-unit orders should drop a stale group-speed
  cap from a previous formation order.

### Tests
Extend `formation_tests.cpp`'s `IssueFormationMoveFP` test block: construct a squad with mixed
`speed` values, issue with `slowestSpeed=true`, assert every unit's `speedCapPixelsPerSec` equals
the minimum. Extend `movement_tests.cpp` or `footprint_tests.cpp` (whichever more directly tests
`UpdateUnitMovement`) with a case asserting a capped unit advances at the capped rate, not its own
`speed`, and that a *new* single-unit order clears the cap.
