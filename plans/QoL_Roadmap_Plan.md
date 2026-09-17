# QoL Feature Roadmap — Index & Shared Prerequisites

## Implementation status (updated 2026-09-17)

All 10 prerequisites resolved as designed (P1/P5/P6 built in `f6d6392`;
P2–P4/P7–P10 settled per-plan during implementation — see each plan).
Shipped, each green on the full suite (`conflict-converge-test.exe`,
soak trajectories byte-identical unless noted):

- Plans 3 + 4 fully implemented, no deviations.
- Plan 1: everything except double-click select-type.
- Plan 2: everything except the area-repair variant (single-target
  repair + area build shipped; reclaim deferred as decided in P9).
- Plan 5: everything except range preview on selection.
- Plan 6: replays + map editor shipped; hotkey remap + color-blind open.
- Standing rule learned mid-implementation: sim-side changes
  (economy/combat timing) each get an isolated full-suite run — the
  Medium-vs-Hard soak margin is ~2% and one attempted change there
  (ScoutTick fast-retry) flipped it and was reverted (see `ea2b6d7`).

Source list: `missing_features.md` (repo root; verified absent against
`src/game` as of its writing). This roadmap splits that list into
6 implementation plans, each a standalone document
another agent can pick up independently:

1. [QoL_Selection_And_Control_Plan.md](QoL_Selection_And_Control_Plan.md) — control groups,
   auto-add-to-group, double-click select-type, line-draw formation, slowest-speed toggle.
2. [QoL_Orders_And_Queueing_Plan.md](QoL_Orders_And_Queueing_Plan.md) — shift-queued order chains,
   attack-ground/force-fire, area commands (repair/reclaim/build).
3. [QoL_Unit_Automation_Plan.md](QoL_Unit_Automation_Plan.md) — overkill protection, target
   priority, player auto-retreat, building auto-repair.
4. [QoL_Production_Economy_Plan.md](QoL_Production_Economy_Plan.md) — repeat production queue,
   select-idle workers/army, select-all-of-type/production hotkeys.
5. [QoL_Camera_Minimap_Info_Plan.md](QoL_Camera_Minimap_Info_Plan.md) — attack/event pings with
   camera jump, right-drag camera pan, range preview.
6. [QoL_Meta_Accessibility_Plan.md](QoL_Meta_Accessibility_Plan.md) — remappable hotkeys,
   color-blind mode, replays/spectator, map editor/modding.

Each plan is self-contained (file:line citations, exact structs/functions to add, test guidance)
so it can be handed to a fresh agent with no other context beyond the repo itself.

## Read this first: shared prerequisite infrastructure

Three research passes over the actual source (not just the doc) turned up **real architectural
gaps** that several features above depend on. Building these once, generically, avoids five
different half-solutions. Each is called out again in the plan(s) that need it, with a link back
here.

### P1. No Ctrl-modifier support anywhere in input
`InputManager` (`src/game/public/app/InputManager.h`) tracks `ShiftDown()` only — there is no
`CtrlDown()`, and `ShortcutRegistry` (`src/game/public/app/Shortcuts.h:35-41`) supports exactly two
bindings per key (`plain`, and a Shift-`chord`), no Ctrl chord slot.
**Needed by:** control groups (Ctrl+number assign), attack-ground (if bound to Ctrl+right-click).
**Fix once:** add `ctrlDown_`/`CtrlDown()` to `InputManager` mirroring `shiftDown_`/`ShiftDown()`
(`InputManager.h:37,47`, `InputManager.cpp:15,55-58`). Do **not** try to extend `ShortcutRegistry`
itself to a 3-way plain/shift/ctrl chord table for the number-key case — control groups need
3-way disambiguation (assign/recall/add) that doesn't fit the registry's binary chord model
cleanly; poll number keys directly in `Game::Update()`'s existing gesture block instead (see
plan 1).

### P2. No canonical "order finished" hook
Every `Issue*Order` function (`Unit.cpp`) overwrites `Unit`'s single set of order fields, but
*completion* is detected ad hoc in multiple places (arrival inside `UpdateUnitMovement`, the
`Space`-halt handler in `Game.cpp:341-363`, the repair-target-lost branch in `Unit.cpp`). There is
no single place that fires "this unit's order just ended."
**Needed by:** order queueing (dequeue-next-on-completion), slowest-speed toggle (reset the speed
cap on completion so it doesn't leak into the unit's next, unrelated order).
**Fix once:** do not try to unify all the existing clear-sites into one callback (too invasive,
high regression risk for a QoL pass). Instead, each feature's plan lists the exact existing
clear-sites it must also touch. If a third feature later needs this hook, *then* it's worth
centralizing — don't build it speculatively now.

### P3. No order queue — `Unit` has exactly one pending order, of any type
Confirmed: `moveTarget/hasMoveOrder`, `attackMove/attackMoveDest`, `hasPatrol/patrolA/patrolB`,
`hasRepairOrder/repairTarget` are all mutually-exclusive single slots; every `Issue*Order` clears
the others. See plan 2 for the `QueuedOrder` struct design.
**Needed by:** shift-queued order chains (plan 2). Attack-ground and area commands are new order
*types*, not queueing — they don't need this themselves, but should be designed with
`QueuedOrder::Kind` extensibility in mind (see plan 2 §1).

### P4. No building selection concept at all
`Building` (`src/game/public/economy/Building.h:32-45`) has no `isSelected` field, and
`Selection.h/.cpp` only ever operates on `Unit`. There is no `PickBuildingAt`.
**Needed by:** per-building auto-repair toggle UI (plan 3), "select-all-production" hotkey
(plan 4), area-build placement UI (plan 2). **Recommendation:** don't build a full building
selection system as a prerequisite chore — each consuming plan below proposes the minimum viable
scope (usually: act on *all* owned buildings of a type, not a specifically clicked one) and flags
where a real click-to-select-building system would be a larger follow-up.

### P5. No right-button hold/drag tracking
`InputManager` has `LeftDown()` (level) but only `RightPressed()` (edge) — no `RightDown()`.
**Needed by:** line-draw formation drag (plan 1), right-drag camera pan (plan 5), area-command
drag (plan 2).
**Fix once:** add `rightDown_`/`RightDown()` to `InputManager`, mirroring `leftDown_`/`LeftDown()`
exactly (`InputManager.h:34,42-48`; `InputManager.cpp:11-28` `Snapshot`/`PollLive`). Add the new
bool as a trailing `Snapshot(...)` parameter with a default, so existing test call sites in
`tests/unit-tests/input_manager_tests.cpp` don't need touching unless they want to exercise it.

### P6. No shared "dragged world rect" helper
The screen→world drag-box conversion (`camera.ScreenToWorld` × 2 + `NormalizeRect`) is inlined at
the box-select call site (`Game.cpp:631-635`) and would otherwise get re-copy-pasted by line
formation, area commands, and double-click's on-screen rect.
**Fix once:** factor out `Rectangle DraggedWorldBox(const GameCamera &camera, Vector2 screenA,
Vector2 screenB)` (pure function, easily unit-tested) into `Selection.h/.cpp` next to
`NormalizeRect` (`Selection.h:18`). Every plan below that needs a world rect from a drag should
call this instead of re-deriving it.

### P7. No damage-event dispatch (spawn/death events exist, damage doesn't)
`EventType::UnitDamaged` is reserved in the enum (`Event.h:21-22`, comment: *"combat and gather
ticks stay dispatcher-free for now"*) but nothing dispatches it. `Combat.cpp`'s `ResolveAttack`/
`ResolveBuildingAttack` are dispatcher-free free functions.
**Needed by:** attack/event pings (plan 5).
**Decision needed (flagged, not resolved here):** thread `EventDispatcher&` into `Combat.h`'s
functions (breaks their current pure-function signature, touches every call site), or reuse the
existing per-frame `hitFlashTime` poll in `Game.cpp:709-730` (detect "just hit" without touching
Combat.h at all). Plan 5 recommends the poll approach as lower-risk for a QoL pass.

### P8. No player-driven building placement flow
`PlaceBuilding` (`Building.h:60-63`) is only ever called from AI/skirmish setup code, never from
`Game.cpp` in response to player input. There is no ghost/preview-follows-cursor state machine.
**Needed by:** area-build command (plan 2), and implicitly by range-preview-on-placement (plan 5,
which is scoped down to selection-only range preview for exactly this reason).
**Not building this as a generic prerequisite** — plan 2 designs the minimal single-placement flow
it needs inline, since "area build" is the only feature that actually requires it right now.

### P9. No wreck/reclaim system
Unit/building death never spawns a wreck entity; there is nothing to "reclaim." This is a bigger,
separate feature, not a QoL wiring gap.
**Decision:** plan 2 scopes "area commands" down to repair + build, and documents reclaim as
explicitly deferred pending a wreck/salvage system design (out of scope for this roadmap).

### P10. No fixed-timestep simulation (blocks true input-recording replay)
Every system integrates on raylib's variable `GetFrameTime()` — no accumulator, no fixed tick.
Deterministic lockstep replay (log commands, re-simulate) requires bit-identical simulation across
runs, which this loop cannot currently guarantee.
**Decision:** plan 6 scopes "replays" down to periodic full-state snapshots (reusing the existing
`SaveWorld`/`LoadWorld` pipeline verbatim) rather than command-log replay, and documents true
lockstep replay as a much larger follow-up requiring a sim-loop rewrite — explicitly out of scope
here.

## Suggested implementation order

Features are independent enough to implement in any order, but this sequencing minimizes rework:

1. **Input plumbing first** (P1, P5, P6) — cheap, mechanical, unblocks everything else. ~half a day.
2. **Plan 3 (unit automation)** — no UI/input dependencies at all, purely simulation-side, safest
   to land first and easiest to regression-test against the existing soak tests.
3. **Plan 4 (production/economy)** — mostly additive to `ProductionQueue`/`Selection`, low risk.
4. **Plan 1 (selection & control)** — depends on P1/P5/P6.
5. **Plan 2 (orders & queueing)** — depends on P3, benefits from plan 1 landing first (squad-scan
   fix noted in plan 1 affects how queueing should read the current selection).
6. **Plan 5 (camera/minimap/info)** — depends on P6/P7, independent of the others otherwise.
7. **Plan 6 (meta/accessibility)** — hotkey remapping and color-blind mode are independent and
   cheap; replay and map editor are large, independent efforts best scheduled last and separately.

## Cross-cutting conventions to preserve (all plans must follow these)

- **Pure logic vs. draw calls stay separated.** Every existing system splits testable pure
  functions (math, parsing, state transitions) from untested raylib draw calls living in
  `Game.cpp`'s render section or thin `Art`/`Hud` wrapper methods. New features must do the same:
  write the logic as a free function you can unit-test without a window, then a thin draw call.
- **New order types follow the Stance/Patrol/Repair recipe** (fields on `Unit` → free
  `Issue*Order` function that clears every other order's fields → a priority-ordered branch inside
  `UpdateUnit` → a keybinding in `Game::BindShortcuts` → a clear-line in the `Space`-halt handler →
  a `RunXTests()` file registered in `tests/main.cpp`). See plan 2 §0 for the full recipe with
  line citations.
- **Every new test file follows the existing pattern**: `tests/unit-tests/<name>_tests.cpp`
  defining `void Run<Name>Tests()`, declared and called from `tests/main.cpp` (see any existing
  entry, e.g. `RunFormationTests`). Build via `premake5.exe vs2022` then MSBuild the
  `conflict-converge-test` project (`Debug|Win32`) — **not** `x64`, the generated solution only has
  Win32 configs. Run `prj/bin/Debug/conflict-converge-test.exe` and confirm `failures: 0` before
  calling any feature done.
- **Don't touch save-format enums carelessly.** Several enums have "keep last: save decode
  validates < Count" comments (`Unit.h`, `UnitType`, etc.) — appending new values is fine, but only
  at the end, never inserting/reordering.
