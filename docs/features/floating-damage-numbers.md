# Floating Damage Numbers

Shipped: 2026-09-17 (commit `bd82f1b`). No deviations.

Hits spawn a rising, fading damage number; the pool lives in `Art`,
spawned from the sim (not the draw loop — pool mutation belongs in
update).

## Behavior

- `DamageNumbers` pool (256 cap, 0.7s life, 40px/s rise) with
  spawn/update/draw/clear.
- Spawned once per hit on the hit-flash edge trigger (`hitFlashTime`
  crossing 0.20s); hit flash overlay itself untouched and independently
  tunable.

## Key files

- `src/game/public/app/Art.h` — pool + `DamageNumber` record.
- `src/game/private/units/Combat.cpp` — hit stamp
  (`lastDamageTaken`/`hitFlashTime`).
- `src/game/private/core/Simulation.cpp` — edge-trigger spawn.
- Tests: `Art` suite (drift half-life, expiry, cap).

## Decisions

- Life/drift are independent tunables; the flash decay path was
  deliberately left alone.
