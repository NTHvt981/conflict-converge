# Floating Damage Numbers

Shipped: 2026-09-17 (commit `bd82f1b`). No deviations.

Hits spawn a rising, fading damage number; the pool lives in the
`Vfx` subsystem, spawned from the sim (not the draw loop — pool
mutation belongs in update).

## Behavior

- `Vfx` damage-number pool (256 cap, 0.7s life, 80px/s rise) with
  spawn/update/draw/clear.
- Spawned once per hit on the hit-flash edge trigger (`hitFlashTime`
  crossing 0.20s); hit flash overlay itself untouched and independently
  tunable.

## Key files

- `src/game/public/app/ui/Vfx.h` — pool + `DamageNumber` record.
- `src/game/private/app/ui/Vfx.cpp` — pool implementation.
- `src/game/private/core/Simulation.cpp` — edge-trigger spawn.
- Tests: `Vfx` suite (drift half-life, expiry, cap).

## Decisions

- Life/drift are independent tunables; the flash decay path was
  deliberately left alone.
