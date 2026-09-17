# Plan: Double-Click Selects All Units of Type On Screen

## Implementation status: SHIPPED (2026-09-17, commit `42c3556`)

- As designed: click-timing state + `DoubleClicked()` in `InputManager`
  (0.35s + 8px, pair-consuming, trailing `nowSeconds` param for tests),
  `SelectAllOfTypeInRect` reusing `SelectInRect`'s containment, wired
  downstream of the placing/minimap/rally guards. No deviations.
- Tests: double-click edge/reject cases + viewport/type/team selection.

The last unshipped item from `plans/QoL_Selection_And_Control_Plan.md` §2. That doc's original
sketch is now stale (written before control groups, right-drag pan, and several other gestures
landed) — this plan re-grounds it against the current code.

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) for the surrounding conventions before starting.

---

## Current state (verified fresh)

- `InputManager` (`src/game/public/app/InputManager.h`, `src/game/private/app/InputManager.cpp`)
  has no click-timing state at all today — its full field list is `mouseScreen_`,
  `prevMouseScreen_`, `mouseDelta_`, `hasPrevMouse_`, `leftPressed_`, `rightPressed_`,
  `wheelDelta_`, `shiftDown_`, `leftDown_`, `rightDown_`, `ctrlDown_`. `MouseDeltaScreen()` exists
  (added for right-drag pan). `Snapshot()`'s current signature:
  ```cpp
  void Snapshot(Vector2 mouseScreenPos, bool leftPressed, bool rightPressed,
               float wheelDelta = 0.0f, bool shiftDown = false, bool leftDown = false,
               bool rightDown = false, bool ctrlDown = false);
  ```
- **`SelectAllOfType` already exists and is exactly the function to reuse** —
  `Selection.h:59-61` / `Selection.cpp:210-229`:
  ```cpp
  int SelectAllOfType(Registry &registry, UnitType type, int teamID, bool add);
  ```
  Already used for the `C` hotkey (`Game.cpp:420-432`): pick `SelectedUnit(registry)`'s type, call
  `SelectAllOfType(registry, unit->type, 0, false)`. This is not restricted to "on screen" — it
  selects the type across the whole map. Double-click needs an on-screen-restricted variant (see
  below).
- Left-click resolution is in `Game.cpp`'s `MenuState::Playing` block:
  - Press (`Game.cpp:946-970`): guarded, in order, by `placingType.has_value()` (area-build drag
    start), `minimap.Contains(...)` (minimap click-to-jump), `settingRally` (rally placement) —
    only falls through to arming a normal `dragging = true` box-select otherwise.
  - Release (`Game.cpp:971-999`): `box.width < 6.0f && box.height < 6.0f` is the existing
    click-vs-drag threshold. On a click: `PickUnitAt` → `SelectOnly(registry, hit)` on a hit, else
    `DeselectAll`. This exact branch (lines 977-983) is where double-click hooks in.
- `PickUnitAt` (`Selection.cpp:7-23`) still has **no team filter** — it returns the first spatial
  hit regardless of team. Any team restriction has to be applied downstream (which
  `SelectAllOfType`'s `teamID` parameter already does).

---

## Design

### 1. Click-timing state in `InputManager`
Add fields mirroring the existing style:
```cpp
// InputManager.h, private:
double lastLeftClickTime_ = -1.0;
Vector2 lastLeftClickPos_ = {};
bool doubleClicked_ = false;
// public:
bool DoubleClicked() const;
```
In `PollLive()`, on a fresh `IsMouseButtonPressed(MOUSE_BUTTON_LEFT)`, compare `GetTime()` against
`lastLeftClickTime_` and the click position against `lastLeftClickPos_` (small pixel tolerance,
~8px) — within a double-click window (e.g. 0.35s) and close enough sets `doubleClicked_ = true`
for that frame only (edge semantics, like `leftPressed_`); otherwise record the new time/pos as
the first click of a potential pair.

For testability, don't call `GetTime()` inside a function the test suite can't control — add a
trailing `double nowSeconds = 0.0` parameter to `Snapshot()` (mirrors how `rightDown`/`ctrlDown`
were added as trailing params with defaults, so no existing call site needs updating), and have
`PollLive()` pass the real `GetTime()` into it. Tests then drive `Snapshot(..., nowSeconds)`
directly with explicit values, exactly like `input_manager_tests.cpp` already does for the other
edge/level flags.

### 2. On-screen + type + team selection function
`SelectAllOfType` selects across the whole map — double-click needs to additionally restrict to
the current viewport. Add a new function in `Selection.h/.cpp` rather than overloading
`SelectAllOfType` (keeps the simple whole-map version available for the `C` hotkey unchanged):
```cpp
// Selects every same-team unit of `type` whose body is inside the given
// world-space viewport rect. Mirrors SelectAllOfType's team/type filter
// and SelectInRect's rect-containment test, combined.
int SelectAllOfTypeInRect(Registry &registry, Rectangle worldViewport, UnitType type,
                          int teamID, bool add);
```
Implementation: same iteration/replace-vs-extend shape as `SelectAllOfType`
(`Selection.cpp:210-229`), with an added containment check against `worldViewport` using
whatever hitbox/rect-overlap test `SelectInRect` already uses in the current source (read
`SelectInRect`'s current body in `Selection.cpp` before writing this — reuse its exact
containment test rather than inventing a second one, so double-click and drag-box select agree
about what "the unit is in this rect" means).

Get the viewport rect via `camera.ScreenToWorld({0,0})` and `camera.ScreenToWorld({screenWidth,
screenHeight})` — the same conversion already used for drag boxes (`DraggedWorldBox`,
`Selection.h`/`Selection.cpp`) and available at the `Game.cpp` call site via `GetScreenWidth()`/
`GetScreenHeight()`.

### 3. Wiring
In `Game.cpp`'s click-resolution branch (currently lines 977-983): after `PickUnitAt` succeeds,
check `input.DoubleClicked()`. If true, call `SelectAllOfTypeInRect(registry,
DraggedWorldBox(camera, {0,0}, {(float)GetScreenWidth(), (float)GetScreenHeight()}), hitUnit->type,
0, false)` instead of `SelectOnly(registry, hit)` (team hardcoded to `0`, matching the existing `C`
hotkey's "team 0 is the player" convention). Play `SfxId::Select` either way.

**Ordering with existing guards**: this must sit downstream of the same
`placingType`/minimap/`settingRally` guards that already gate the press at lines 946-970 — a
double-click while placing a building, clicking the minimap, or setting a rally point must not
also trigger a selection change. Since the click-resolution branch (971-999) only runs when none
of those consumed the press (the `dragging` flag is only set in the final `else`, line 966-969),
this is already naturally satisfied — just confirm it's still true when you land the change, since
more gesture guards may have been added by the time this is implemented.

**Team-agnostic `PickUnitAt` interaction**: since `PickUnitAt` doesn't filter by team,
double-clicking an enemy unit will resolve `hit` to that enemy before `SelectAllOfTypeInRect`'s own
`teamID` filter runs — with `teamID` hardcoded to `0`, this means double-clicking an enemy unit
selects nothing (0 count) rather than selecting enemies. That's an acceptable, arguably correct
outcome (you can't command enemy units anyway) but note it explicitly rather than let it be an
accidental side effect — don't try to "fix" `PickUnitAt` as part of this change, it's a
pre-existing, separately-scoped gap noted in other plans.

---

## Tests

- `input_manager_tests.cpp`: two `Snapshot()` calls with close timestamps/positions →
  `DoubleClicked()` true; far timestamp or far position → false. Mirror the existing edge-flag test
  shape in that file.
- `selection_tests.cpp`: construct mixed-type/mixed-team units both inside and outside a fake
  viewport rect; call `SelectAllOfTypeInRect`; assert only same-type/same-team/in-viewport units
  get selected, and that `add=false` replaces prior selection (mirror `SelectInRect`'s existing
  test shape for the replace/extend assertions).
