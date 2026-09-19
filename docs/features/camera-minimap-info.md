# Camera, Minimap and Info

Shipped: umbrella mostly shipped 2026-09-17, including the extracted
range-preview follow-up (`RangePreview_Plan.md`, commit `42c3556`).
Hover range preview and placement-ghost rings were deferred in both plans
and are still absent.

Attack/event pings with camera jump, opt-in right-drag pan, selection
range rings, and the minimap viewport transform.

## Behavior

- Pings: `UnderAttack` (damage poll, team 0) and `UnitLost` (lifecycle
  event) raise minimap blips (5s life, per-kind retrigger floor, 32 cap);
  `J` jumps the camera to the latest; minimap click-to-move.
- Right-drag pan (opt-in persisted setting): past a 6px threshold pans
  zoom-compensated; press defers orders, sub-threshold release still
  dispatches. `Alt`+right-drag stays reserved for line-formation.
- Range preview: selected units with `attackRange > 0` draw a red ring;
  hover shows tooltip text only, no ring; placement ghost shows no ring.
- Minimap: world/minimap transforms both ways, viewport box, click-to-move.

## Key files

- `src/game/public/app/Pings.h` + `private/app/Pings.cpp` — ping store,
  expiry, floor, `Latest()`.
- `src/game/private/core/PlayingInput.cpp` — right-drag pan, click-vs-drag
  order deferral, minimap click-to-move.
- `src/game/private/core/Game.cpp` — range ring, jump hotkey, viewport and
  blip drawing.
- `src/game/public/app/Minimap.h` + `private/app/Minimap.cpp` — transforms,
  viewport rect.
- Tests: `Ping` (raise/expiry/floor/`Latest`), `InputManager`
  (right-drag deltas), `Minimap` (transforms/viewport), `Menu` (setting
  persistence) suites. Draw calls themselves are visual-only by
  convention.

## Decisions

- Damage-poll ping source (no `Combat.h` change) with the lifecycle-event
  payload for deaths — same detector feeds damage numbers and shake.
- Ring color/radius are untested draw calls (project convention: verified
  visually when the game runs).
- Open: hover range preview, placement-ghost range ring.
