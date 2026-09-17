# Plan: Screen/Camera Shake

## Implementation status: SHIPPED (2026-09-17, commit `763b815`)

- As designed: trauma pattern (pure helpers in header-only `Shake.h`),
  decay per frame, squared magnitude, shaken `Camera2D` copy at
  `BeginMode2D` (real camera untouched), hit (+0.2) and wipe (+0.5)
  triggers, math tests in `camera_tests.cpp`. No deviations.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — `FloatingDamageNumbers_Plan.md`, etc. — each independent, no shared
prerequisites).

---

## Current state (verified fresh)

Confirmed zero shake anywhere: a repo-wide grep for `shake|Shake|SHAKE` returns no
matches at all — no field, function, or even a stale reference.

- Camera: `GameCamera` (`src/game/public/app/GameCamera.h:11-53`) wraps a single
  `Camera2D view` (raylib's `offset`/`target`/`rotation`/`zoom`).
- Init: `Game.cpp:144-146` — `view.offset = {kInitialWidth/2, kInitialHeight/2};
  view.rotation = 0.0f; view.zoom = 1.0f;`.
- Per-frame update, `Game::Update()` (`Game.cpp:827-849`):
  1. `input.Update(camera, ..., GetFrameTime())` (830-831) drives WASD/edge-pan via
     `GameCamera::Pan` → `view.target.x/y += delta` (`GameCamera.cpp:27-31`).
  2. `camera.AdjustZoom(...)` (835) mutates `view.zoom`.
  3. **`camera.view.offset = {screenWidth/2, screenHeight/2}` (840) — recomputed from
     scratch every frame.** Do not add shake here; it gets overwritten immediately.
  4. `camera.ClampZoomToWorld(...)` (844-846) and `camera.ClampToMap(...)` (847-849)
     mutate `view.zoom`/`view.target` again, clamping to absolute world bounds
     (`GameCamera.cpp:95-146`). `ClampToMap` reads `view.target`'s *absolute*
     position and snaps it back in-bounds — injecting shake into `view.target` before
     this runs would get partially eaten by the clamp and could permanently drift
     `target` if the shake isn't perfectly symmetric (nothing reverts it).
- Render entry point: `BeginMode2D(camera.view)` at `Game.cpp:1904`, matching
  `EndMode2D()` at `Game.cpp:2139`.
- **Two other consumers read `camera.view` after `EndMode2D()` for screen-space HUD
  math, and must see the unshaken value**: `GetWorldToScreen2D(center, camera.view)`
  (`Game.cpp:2169`) and `minimap.ViewportRect(camera.view, ...)` (`Game.cpp:2188`).

## Design

**Do not mutate `camera.view.target`/`.offset` in `Update()`** — that's the
persistent, clamped "real" camera state `Pan`/`ClampToMap` reason about across
frames. Keep shake entirely separate and apply it only at the render call site via a
local copy:

```cpp
Camera2D shaken = camera.view;
shaken.offset.x += shakeOffset.x;
shaken.offset.y += shakeOffset.y;
BeginMode2D(shaken);
```
right before `Game.cpp:1904`. `camera.view` itself stays untouched, so both
post-`EndMode2D` consumers (`Game.cpp:2169`, `2188`) and next frame's
`Update()`/`ClampToMap` keep operating on the real, unshaken camera.

1. **New transient state**, e.g. a small struct or two fields on `Game`
   (`src/game/public/core/Game.h`, alongside other transient timers like
   `attackSfxTimer`): `float shakeTrauma = 0.0f;` (0-1, decayed over time) or simpler
   `float shakeTimeRemaining, shakeMagnitude`. Prefer the "trauma" pattern (a 0-1
   value that decays each frame, squared for the actual offset magnitude) since it
   composes cleanly when multiple shakes overlap (just take the max, or add and
   clamp to 1) — a simple countdown timer doesn't compose as gracefully if two
   explosions land close together.
2. **Decay once per frame** in `Update()`, same idiom as `Pings::Update` ages pings
   (`src/game/public/app/Pings.h`, `Pings.cpp`) — `shakeTrauma = std::max(0.0f,
   shakeTrauma - decayRate * dt);`.
3. **Compute the offset** right before `BeginMode2D`: `float amount = shakeTrauma *
   shakeTrauma; shakeOffset = { RandomJitter(amount), RandomJitter(amount) };` (a
   simple `GetRandomValue`-based jitter is fine; a Perlin-noise-driven shake is a
   nicer-later upgrade, not needed for v1).
4. **Trigger call sites**: add `shakeTrauma = std::min(1.0f, shakeTrauma +
   amount)` alongside existing impact moments — the hit-particle spawn
   (`Game.cpp:1673-1685`) for small hits, the death-burst spawn (`Game.cpp:1654-1657`)
   for a bigger jolt, and optionally building destruction if that has its own event.
   Tune per-trigger `amount` so death/big-explosion shakes noticeably more than a
   routine hit.

## Tests

Camera-shake math is pure and testable without rendering: a small `camera_shake`
section in `camera_tests.cpp` (or wherever `GameCamera` is already tested) —
confirm trauma decays to 0 over time, confirm `camera.view` itself (the real,
persistent state) is never mutated by shake (spawn trauma, advance frames, assert
`view.target`/`view.offset` unchanged — only a local/rendered copy should differ).
