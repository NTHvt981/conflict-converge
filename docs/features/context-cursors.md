# Context Cursors

Shipped: 2026-09-17 (commit `a1948d2`). No deviations.

The cursor predicts intent per frame from the hovered tile and switches
the OS cursor before drawing.

## Behavior

- `PredictCursorIntent` classifies hover: enemy → Attack, repairable
  friendly (Engineers) → Repair, else Move; null → Default.
  Attack-ground mode forces Attack; placement validity flows through
  `CanPlaceBuilding` (invalid → not-allowed cursor).
- Mapped to built-in cursors (crosshair / pointing-hand / not-allowed /
  arrow); custom icon textures were v1-out-of-scope by design.

## Key files

- `src/game/public/app/Cursor.h` + `private/app/Cursor.cpp` — intent
  enum + pure prediction (takes non-const `Registry&`, documented:
  `PickUnitAt` is non-const).
- `src/game/private/core/Game.cpp` — per-frame classification and
  `SetMouseCursor` wiring.
- Tests: `Cursor` suite (`cursor_tests.cpp`: null, enemy, ground,
  engineer-repair priority).

## Decisions

- Prediction is pure and per-frame-safe; no validity concept in the
  editor (out of scope), so no invalid-cursor there.
