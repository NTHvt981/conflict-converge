# Menu Transitions

Shipped: 2026-09-17 (commit `bd04869`). One simplification: a uniform
fullscreen fade-from-black overlay instead of per-screen `Fade()` wraps
(same visual, a handful of lines).

Menu changes fade through black on a pure clock owned by `Game`.

## Behavior

- `TrackMenuTransition` + `MenuFadeAlpha` over `kMenuFadeInDuration`;
  `previousMenuState`/`menuStateTime` ticked per frame; overlay drawn
  over menu screens and world overlays alike.
- Slide/scale-pop transitions explicitly deferred to a follow-up.

## Key files

- `src/game/public/app/Menu.h` — pure transition clock.
- `src/game/public/core/Game.h` + `private/core/Game.cpp` — clock
  ownership and tick.
- `src/game/private/core/MenuScreens.cpp` — fade overlay.
- Tests: `Menu` suite (reset/accumulate/clamp).

## Decisions

- State owned by `Game`, not `MenuFlow` (flow stays stateless).
