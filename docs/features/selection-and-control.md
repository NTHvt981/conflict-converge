# Selection and Control

Shipped: umbrella fully shipped 2026-09-17, including the extracted
double-click follow-up (`DoubleClick_SelectType_Plan.md`, commit `42c3556`).

Click/drag-box selection, control groups, double-click select-type, and
line-draw formations — all gestures dispatch through `PlayingInput`, with
pure helpers in `Selection`/`Formation` covered headless.

## Behavior

- Click selects, drag-box selects (`Shift` extends); sub-6px drags resolve
  as clicks (`SelectInRect`, center-hit).
- Control groups: `Ctrl+N` assign, `Shift+N` add, `N` recall,
  multi-membership via `Unit.controlGroups` bitmask; factory output
  auto-adds to a group armed with `Ctrl+Shift+N`.
- Double-click selects all same-type units on screen (0.35s / 8px pair
  window, team 0); whole-map variant stays on `C`.
- Line-draw formation: `Alt`+right-drag previews a line, release issues
  per-unit slots (`IssueLineFormationMoveFP`).
- Slowest-speed toggle (`B`) caps the selection to its slowest member
  (`EffectiveSpeed`); cleared on new orders and `Space` halt.

## Key files

- `src/game/public/app/Selection.h` + `private/app/Selection.cpp` —
  pick/box/line queries, group ops, `SelectAllOfType(InRect)`.
- `src/game/public/app/InputManager.h` — `Ctrl`/`Right` levels,
  `DoubleClicked` timing (injectable clock, headless-tested).
- `src/game/public/core/PlayingInput.h` + `private/core/PlayingInput.cpp` —
  all gesture wiring.
- `src/game/public/units/Formation.h` — grid + line formation moves,
  slowest-speed set-point.
- Tests: `Selection`, `ControlGroup`, `InputManager`, `Formation`,
  `LineFormation` suites.

## Decisions

- Group membership is a bitmask, not a single id (multi-membership free).
- Gestures live in `PlayingInput`, not inline `Game::Update` (extracted
  during the GameUpdate decomposition).
- `Alt`+right-drag for lines, with a right-drag-pan guard, so pan and
  formation never arm together.
