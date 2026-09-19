# Facing

Shipped: 2026-09-17 (commit `857d759`). One extension beyond plan scope:
prototype infantry got directional anims too.

Units face 8 directions from velocity; the renderer picks the matching
animation column, falling back to legacy sheets.

## Behavior

- `Unit.facing` (8-way, default Right), maintained from nonzero velocity
  (`FacingFromVelocity`) and kept on stop.
- `Art::UnitSprite` resolves `<prefix>_walk_<dir>` / `<prefix>_idle_0_<dir>`
  with legacy fallback and column clamp; render passes `unit.facing`.
- 8 `infantry_walk_0..7` anims in `data/configs/animations.json` (the dead
  `infantry_idle` anim is gone); the unit editor previews facings 0–7.

## Key files

- `src/game/public/units/Unit.h` — `Facing` enum + field.
- `src/game/private/units/Unit.cpp` — velocity mapping + driver upkeep.
- `src/game/public/app/Art.h` — directional lookup.
- Tests: `Movement` (octants/boundaries/march/stop), `Art` (lookup/clamp),
  `SpriteData` (real-file anims) suites.

## Decisions

- Out of scope by design: attack-facing, flat-PNG/rect facing,
  per-direction timings.
