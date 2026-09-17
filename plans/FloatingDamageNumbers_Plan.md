# Plan: Floating Damage Numbers

## Implementation status: SHIPPED (2026-09-17, commit `bd82f1b`)

- As designed: `DamageNumbers` pool mirroring `Particles`
  (`Art.h/.cpp`, 0.7s life, 40px/s rise), `hitFlashTime > 0.20f` edge
  trigger, world-space draw under the shroud, pool tests in
  `art_tests.cpp`.
- One deviation: spawning happens in the update loop next to the
  particle burst, not in the draw loop — pool mutation belongs in
  update; same trigger and anchor either way.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — `ScreenShake_Plan.md`, `BuildingConstructionProgress_Plan.md`, etc. — each
independent, no shared prerequisites).

---

## Current state (verified fresh)

- `Unit::lastDamageTaken`/`Unit::hitFlashTime` fields: `src/game/public/units/Unit.h:138-139`.
- Set on hit in `ResolveAttack`: `src/game/private/units/Combat.cpp:31-32`
  (`defender.lastDamageTaken = effective; defender.hitFlashTime = kHitFlashDuration;`),
  duration constant `kHitFlashDuration = 0.25f` (`src/game/public/units/Combat.h:31`).
- Decremented once per frame, straight linear countdown, in `UpdateUnit`
  (`src/game/private/units/Unit.cpp:717-724`).
- Draw call — fixed position, binary appear/disappear, no drift or fade —
  `src/game/private/core/Game.cpp:2100-2104`:
  ```cpp
  if (unit.hitFlashTime > 0.0f)
  {
      DrawRectangleRec(body, Fade(WHITE, 0.7f));
      DrawText(TextFormat("-%.0f", unit.lastDamageTaken), static_cast<int>(body.x),
               static_cast<int>(body.y) - 18, 16, RED);
  }
  ```
  The number and the white hit-flash overlay share the same 0.25s timer — that's the
  key constraint below.
- A different consumer of the same flag: `Game.cpp:1673-1685` spawns a 6-particle
  yellow burst and raises a ping when `hitFlashTime > 0.20f` (an edge-trigger window
  in the first 0.05s, not the flash's full duration) — unrelated to this plan, don't
  touch it.

### The reusable pattern already in this codebase

`Particles`/`Particle` (`src/game/public/app/Art.h:54-77`, impl
`src/game/private/app/Art.cpp:107-161`) already does exactly "spawn at a world
position, drift by a stored velocity, fade by remaining-life-over-max-life":
```cpp
struct Particle { Vector2 pos, vel; float life, maxLife, size; Color color; };
// Update: p.life -= dt; erase when life <= 0; else pos += vel * dt
// Draw: alpha = p.life / p.maxLife; DrawCircleV(p.pos, p.size, Fade(p.color, alpha));
```
Wired into the frame loop already: spawned at `Game.cpp:1655-1657`/`1677`/`1695-1697`,
updated once per frame (`Game.cpp:1702`, `art.ParticlesPool().Update(dt)`), drawn
inside the `BeginMode2D`/`EndMode2D` world-space block (`Game.cpp:2121`,
`art.ParticlesPool().Draw()`).

This is a circle-drawing pool, not text, so it can't be reused verbatim — but the
struct-of-`{pos, vel, life, maxLife}` + `Update`/`Draw` shape is the exact template to
copy.

## Design

**Don't tie the number's lifetime to `hitFlashTime`.** That field is shared with the
white body-flash overlay and is only 0.25s — reusing it directly pins the number to a
duration tuned for the flash, not for a readable floating-number animation (which
typically wants ~0.6-1.0s to drift and fade).

1. New small pool, e.g. `DamageNumbers` (mirror `Particles`' file placement — a
   sibling struct/class in `Art.h`/`.cpp`, or its own `DamageNumbers.h`/`.cpp` next to
   it if `Art.h` is getting crowded):
   ```cpp
   struct DamageNumber { Vector2 pos; float value; float life, maxLife; };
   class DamageNumbers {
   public:
       void Spawn(Vector2 pos, float value);
       void Update(float dt);   // pos.y -= driftSpeed * dt; life -= dt; erase at 0
       void Draw() const;       // alpha = life/maxLife; DrawText(TextFormat("-%.0f", value), ...)
   };
   ```
2. Spawn point: alongside the existing `hitFlashTime` set in `ResolveAttack`
   (`Combat.cpp:31-32`) — but `Combat.cpp` doesn't have access to a screen-space
   `body` rect (that's computed later, per-frame, in `Game.cpp`'s draw loop). Simplest
   correct approach: spawn in `Game.cpp`'s draw loop instead, using the exact same
   `body`-relative position the current fixed text uses, but only spawn **once per
   hit** — guard with an edge-trigger the same way the existing particle-burst code
   at `Game.cpp:1673-1685` does (`hitFlashTime > 0.20f`, since `hitFlashTime` is set
   to its max exactly once per hit and decays afterward, that threshold window is a
   reliable "just got hit this frame" signal already proven by that call site).
3. Update/draw wiring: same pattern as `Particles` — `Update(dt)` once per frame near
   `Game.cpp:1702`, `Draw()` inside the `BeginMode2D`/`EndMode2D` block near
   `Game.cpp:2121` (world-space, so the number scrolls/scales with the camera
   correctly, matching the existing fixed-position text's behavior).
4. Tune `maxLife` (~0.6-0.8s) and upward drift speed independently of
   `kHitFlashDuration` — they no longer need to match.
5. Leave the existing white hit-flash overlay (`DrawRectangleRec(body, Fade(WHITE,
   0.7f))`) and its 0.25s timer completely alone; only the damage-number draw line
   moves into the new pool.

## Tests

Follow `art_tests.cpp`'s existing style for `Particles` (if it has direct pool tests)
or add a small `damage_numbers_tests.cpp`: spawn one, advance `Update` past half its
`maxLife`, confirm position has moved (drift) and is still present; advance past
`maxLife`, confirm it's removed. No world/registry dependency needed — this is a pure
pool, testable in isolation like `Particles` should be.
