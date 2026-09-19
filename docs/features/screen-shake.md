# Screen Shake

Shipped: 2026-09-17 (commit `763b815`). No deviations from the plan.

Impacts shake the rendered view without touching camera state: a 0..1
trauma value on `Game` is raised by hits and deaths, bleeds off per frame,
and jitters a camera copy at render time.

## Behavior

- `Game::shakeTrauma` (0..1), raised additively and clamped
  (`AddShakeTrauma`): +0.2 routine hits, +0.5 unit deaths. Overlapping
  shakes compose instead of restarting.
- Decays per frame (`DecayShakeTrauma`, 1.5 trauma/s — a full hit clears
  in ~0.7s).
- Rendered offset scales with trauma squared (`ShakeMagnitude`, 12px max):
  hits stay sub-pixel (felt, not seen), wipes jolt ~3px. Jitter is uniform
  via `GetRandomValue`, not noise.
- Applied to a COPY at `BeginMode2D`. `camera.view` itself is never
  mutated, so `Pan`/`ClampToMap` and the post-`EndMode2D` HUD math
  (world-to-screen, minimap viewport) keep reasoning about the real camera.

## Key files

- `src/game/public/app/Shake.h` — pure header-only math, headless-safe.
- `Game::shakeTrauma` field (`src/game/public/core/Game.h`); decay in
  `Game.cpp`; hit/death triggers in `Simulation.cpp` (moved there by the
  GameUpdate decomposition — the plan cited `Game.cpp` impact sites).
- Tests: trauma section in `tests/unit-tests/camera_tests.cpp` (add/clamp,
  decay floor, squared magnitude).

## Decisions

- Trauma pattern over a countdown timer: overlapping explosions compose
  (add + clamp); a timer would restart and lose the first hit.
- Copy-at-render over mutating `view`: injecting offsets into the
  persistent camera would get partially eaten by `ClampToMap` and could
  drift `target` permanently.
