# Game Update Decomposition

Implemented: slices 1–4 plus the `Simulation` follow-up
(commits `07add71`/`b2ec043`/`fd166a0`/+slice4). Went beyond the plan's
"no new files" constraint (see Decisions).

`Game::Update` is a thin orchestrator: sim ticks in `Simulation`, input
dispatches in `PlayingInput`, menu draws in `MenuScreens`, and render
splits at `EndMode2D`.

## Behavior

- `Simulation::Step` owns the per-frame match tick (construction,
  retreat, factory refresh, replay counters, match reset/stop).
- `PlayingInput::Dispatch` owns all gesture handling, held by `Game`.
- `MenuScreens::Draw` owns the menu branch, held by `Game`.
- Render: `Game::DrawWorld` + `Game::DrawHudAndOverlays`, split at
  `EndMode2D`.

## Key files

- `src/game/public/core/Simulation.h` + `private/core/Simulation.cpp`.
- `src/game/public/core/PlayingInput.h` + `private/core/PlayingInput.cpp`.
- `src/game/public/core/MenuScreens.h` + `private/core/MenuScreens.cpp`.
- `src/game/public/core/Game.h` — ownership (`playingInput`,
  `menuScreens`).
- Tests: `Simulation` suite; the e2e binary stays the wiring gate.

## Decisions

- The sim graduated from a `Game` method to its own `Simulation` class
  (with replay/edge-poll API), and input/menu got their own TUs — more
  files than planned, but each unit now has one job and one test path.
