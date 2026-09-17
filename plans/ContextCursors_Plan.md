# Plan: Context-Sensitive Mouse Cursors

## Implementation status: SHIPPED (2026-09-17, commit `a1948d2`)

- As designed: `CursorIntent` + `PredictCursorIntent` (`Cursor.h/.cpp`,
  new TUs), per-frame classification before `BeginMode2D`, placement
  via `CanPlaceBuilding`, built-in raylib cursors for v1,
  `cursor_tests.cpp`.
- Two deviations: the function takes `Registry&`, not `const Registry&`
  (`PickUnitAt` is non-const); attack-ground mode also shows the attack
  cursor since the next right-click shells the point.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — each independent, no shared prerequisites).

---

## Current state (verified fresh)

Confirmed zero cursor-shape code: a repo-wide grep for `SetMouseCursor|MouseCursor|
CURSOR_` (case-sensitive) and `cursor` (case-insensitive) returns no hits related to
cursor appearance anywhere in `src/` or `plans/`.

### Input tracking available to build on

`InputManager` (`src/game/public/app/InputManager.h`, `src/game/private/app/
InputManager.cpp`) already tracks per frame: `MouseScreen()`, `MouseWorld(camera)`
(`InputManager.cpp:113-116`, via `GameCamera::ScreenToWorld`), click edges/levels,
modifiers, double-click. **No cursor-shape field or "what's under the cursor"
classification exists here** — new state is needed, most naturally computed once per
frame in `Game.cpp`'s main loop after `input.Update(...)`.

### Key finding: right-click does NOT currently distinguish attack from move at all

The "what would right-click do" logic lives inline inside the lambda
`dispatchRightClickOrders` (`Game.cpp:1412-1538`). Reading it fully:
- `Game.cpp:1441-1509` (single-unit branch): checks only `ordered->type ==
  UnitType::Engineer` + `CanRepairTarget` (via `PickUnitAt`) to pick Repair vs. plain
  Move. **No team/enemy check anywhere in this lambda.**
- `Game.cpp:1510-1538` (squad branch): Shift-queue Move or formation move — again no
  enemy check.

Actual combat is **not** triggered by the click — it's automatic per-frame
acquisition inside `UpdateUnit` (`AcquireTarget`/`AcquireBuildingTarget`,
`src/game/private/units/Unit.cpp:802,805,863,877`), gated by range, not by what was
clicked. A right-click on an enemy today issues a **plain move order onto that
tile**; the unit only starts fighting once auto-acquisition later notices the enemy
in range.

**Implication**: there is no existing "predict right-click intent" function to call
for cursor prediction — it must be written new, not refactored out of existing
branching (since the branching doesn't make this distinction today). New predictive
logic, callable every frame without side effects:
1. `PickUnitAt(registry, worldPos)` (`src/game/public/app/Selection.h:16`, impl
   `Selection.cpp:7-23`) — pure query, cheap enough to call every frame (already used
   outside a strict click context at `Game.cpp:1453`).
2. If a unit is found and `hitUnit->teamID != selectedUnit->teamID` → attack cursor.
3. Else if `ordered->type == UnitType::Engineer && CanRepairTarget(...)` (mirroring
   `Game.cpp:1450-1483`) → repair cursor.
4. Else → move cursor.

For **building placement**, `CanPlaceBuilding(map, nodes, type, tileX, tileY)`
(`Building.h:83`, impl `Building.cpp:100`) is already called every frame purely to
color the ghost preview (`Game.cpp:1971-1979`) — a placement cursor reuses this
exact function with zero refactor.

The **map editor** (`Game.cpp:1081-1183`) has no placement-validity concept at all
(always paints) — "invalid cursor in the editor" has nothing to hook into without
adding new validity logic there first; treat as out of scope for v1 unless the editor
gains bounds/validity checks separately.

### raylib cursor API — available, vendored

`deps/raylib/src/raylib.h`, raylib 6.0:
- `MouseCursor` enum (`raylib.h:722-733`): `DEFAULT`, `ARROW`, `IBEAM`, `CROSSHAIR`,
  `POINTING_HAND`, 4x `RESIZE_*`, `NOT_ALLOWED` — 11 built-in shapes, **no RTS-style
  attack/move icons**.
- `SetMouseCursor(int)` (`raylib.h:1235`).
- `ShowCursor`/`HideCursor`/`EnableCursor`/`DisableCursor` (`raylib.h:1037-1041`) —
  needed only if going the custom-texture route (hide system cursor, draw a texture
  at `GetMousePosition()` each frame).

`NOT_ALLOWED` is a good zero-art fit for invalid building placement. A genuinely
distinct attack-cursor (crosshair-on-target look) needs a custom texture — raylib's
built-in enum has no such shape.

## Scope decision: built-in shapes for v1, custom textures as a follow-up

Ship with raylib's built-in `MouseCursor` values first (`NOT_ALLOWED` for invalid
placement, `POINTING_HAND` or `CROSSHAIR` as a stand-in for "attack", default
`ARROW` for move) — zero art dependency, unblocks the feature immediately. Custom
attack/move icon textures are a pure follow-up (swap the `SetMouseCursor` calls for
a hide-cursor + `DrawTexture` loop) once art exists; don't block v1 on art.

## Design

1. New per-frame classification, computed once after `input.Update(...)` in
   `Game::Update()` (not inside `dispatchRightClickOrders`, since cursor prediction
   must run every frame regardless of whether a click happens): an enum
   `CursorIntent { Default, Move, Attack, Repair, InvalidPlacement }` and a function
   `CursorIntent PredictCursorIntent(Registry&, const Unit* selected, Vector2
   worldPos, ...)` following the logic derived above.
2. Building-placement mode overrides the above entirely: if `placingType.has_value()`
   (per the existing area-build placement flag), classification is purely
   `CanPlaceBuilding(...)` → `InvalidPlacement` or `Default`.
3. Map to `SetMouseCursor(...)` once per frame based on the computed intent — call
   this near the end of `Update()`/start of `Draw()`, after all other input handling,
   so it reflects the final state for this frame.
4. No selection → always `Default` (don't run prediction for an empty selection,
   it's wasted work and there's nothing to act on).

## Tests

`PredictCursorIntent` is a pure function (registry + positions in, enum out) —
straightforward to unit test without any raylib window: hovering an enemy unit with
an attack-capable unit selected → `Attack`; hovering ground → `Move`; an Engineer
hovering a damaged friendly unit → `Repair`; placement mode over blocked terrain →
`InvalidPlacement`. New `tests/unit-tests/cursor_tests.cpp`, registered in
`tests/main.cpp` alongside the other `Run*Tests` declarations.
