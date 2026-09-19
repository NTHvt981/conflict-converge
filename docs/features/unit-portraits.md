# Unit Portraits

Shipped: 2026-09-17 (commit `af51828`). No deviations.

Single selection shows a front-facing idle portrait; multi-select shows
a count. Text-only fallback when atlas art is missing.

## Behavior

- Portrait: `UnitSprite` frame at fixed 1.25x (tuned for 16x32 cells),
  team-tinted; gated on atlas art existing, else text panel.
- Multi-select: `"%d units selected"` via `SelectedUnitCount`.
- Multi-select portrait row deferred until more non-placeholder art
  exists.

## Key files

- `src/game/private/app/Hud.cpp` — selection panel wiring.
- `src/game/public/app/Selection.h` — pure count query.
- `src/game/public/app/Art.h` — `UnitSprite` gate/art source.
- Tests: `Selection` suite (count cases).

## Decisions

- Gate on `UnitSprite()` non-empty rather than raw sprite lookup (no
  new Art API needed).
