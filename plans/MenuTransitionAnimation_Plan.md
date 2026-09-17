# Plan: Menu Transition Animation

## Implementation status: SHIPPED (2026-09-17, commit `bd04869`)

- Pure clock as designed: `TrackMenuTransition`/`MenuFadeAlpha`
  (`Menu.h`, tested in `menu_tests.cpp`), state on `Game` (not
  `MenuFlow`), covers menu screens and in-world overlays.
- One deviation: a uniform fullscreen fade-from-black overlay instead
  of per-screen `Fade()` wrapping — same fade-in visual, a handful of
  lines instead of touching all 7 screen branches.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — each independent, no shared prerequisites).

---

## Current state (verified fresh)

`MenuState`/`MenuFlow` (`src/game/public/app/Menu.h:17-96`): an 11-value enum
(`MainMenu, SkirmishSetup, Settings, HotkeyRemap, LoadGame, Playing, Paused,
GameOver, Victory, ReplayViewer, MapEditor`). `MenuFlow` holds only `state`,
`settings`, `setup`, `quitRequested` — **no timing field anywhere**, and `Menu.cpp`
(262 lines) is pure state mutators/settings I/O with zero raylib/timer includes.

Draw dispatch: `Game::Update()`, `Game.cpp:827`. When `!worldActive` (853):
`BeginDrawing()` (859-860), then a flat `if/else if` chain over `menu.state` from
**line 862 to 1188**, `EndDrawing(); return;` (1189-1190). Every transition (e.g.
`menu.state = MenuState::MapEditor;` at 896) takes effect immediately — the very
next frame's dispatch draws the new branch's full UI from scratch, no interpolation,
fade, or hold-over frame. Even in-world overlays (`Paused`/`Victory`/`GameOver`
windows drawn on top of a live frame, `Game.cpp:2257-2330`) switch the same way.

**No existing per-state-elapsed-time tracking anywhere.** `Game.h` has only
unrelated timers (`replayTimer`, `replayPlayTimer`, `attackSfxTimer`). The closest
thing, `lastOutcomeState` (`Game.h:186`), only detects the transition *edge* for a
one-shot fanfare (`Game.cpp:1863-1876`) — it doesn't carry elapsed time, and can't be
reused directly for animation, only as a reference for "how this codebase already
detects state changes."

### Existing time-based animation pattern to copy

`Pings::age` (`src/game/public/app/Pings.h`) — "seconds since raised; blips
fade/pulse by age" — aged via `Pings::Update(float dt)`, called every frame
regardless of menu state (`Game.cpp:1899`: `pings.Update(GetFrameTime());  // ages in
menus/viewer too`). Consumers read `age` to drive fade/pulse (`Game.cpp:2191-2199`):
```cpp
const float pulse = 2.0f + ping.age * 2.0f;
const Color color = Fade(RED, 1.0f - ping.age / 5.0f);
```
This `age`-incremented-by-`dt`-then-mapped-to-`Fade()`/size shape is the exact
template to reuse for menu transitions.

## Design

1. **New state on `Game`** (not `MenuFlow` — `Menu.h`/`Menu.cpp` currently has zero
   raylib/timing dependency and changing that couples the plain-data settings/state
   struct to frame timing for no benefit; keep the animation clock where the frame
   loop already lives): `MenuState previousMenuState = MenuState::MainMenu;` and
   `float menuStateTime = 0.0f;`.
2. **Detect + reset**, once per frame near the top of the `!worldActive` branch
   (`Game.cpp:853`, before the dispatch chain): if `menu.state != previousMenuState`,
   reset `menuStateTime = 0.0f; previousMenuState = menu.state;`. Otherwise
   `menuStateTime += GetFrameTime();`.
3. **Feed it into the dispatch chain**: the simplest, lowest-risk v1 is a fade-in —
   compute `float alpha = std::min(1.0f, menuStateTime / kFadeInDuration);` (e.g.
   `kFadeInDuration = 0.15-0.25f`) once, then wrap each screen's background/panel
   draws with `Fade(color, alpha)` instead of the raw color. This requires touching
   each of the ~7 screen branches in the 862-1188 chain, but each touch is
   mechanical (multiply existing colors by `alpha` via `Fade`) — no restructuring of
   the chain itself.
4. **Scope decision: fade only for v1, not slide/scale.** A slide or scale-pop
   transition needs per-element position/size math (different for every screen's
   layout), which is a much larger diff across all 7 branches. A uniform fade-in
   achieves "no more instant jarring snap" cheaply and uniformly; treat slide/scale
   as a follow-up once fade is proven out, not a v1 requirement.
5. Apply the same `menuStateTime`-driven fade to the in-world overlays
   (`Paused`/`Victory`/`GameOver`, `Game.cpp:2257-2330`) using the same
   `previousMenuState`/`menuStateTime` pair — they already flow through the same
   `menu.state` value, so no separate tracking is needed.

## Tests

The elapsed-time tracking itself is pure and testable: a small `menu_transition`
section (new or added to `menu_tests.cpp`) — confirm `menuStateTime` resets to 0 the
frame `menu.state` changes, and accumulates correctly across subsequent frames with
the same state. Visual fade correctness (does it look right) is draw-call-only per
this project's established testing split — verify manually when the game is run.
