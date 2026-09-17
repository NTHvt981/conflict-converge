# Plan: Building Construction Progress

## Implementation status: SHIPPED (2026-09-17, commit `0381249`)

- As designed: `UnderConstruction` state, `PlaceBuilding` starts at 0
  HP, `UpdateBuildingConstruction` ramps to exact-max then flips,
  ticked in `Game::Update` beside auto-repair, world-space progress
  bar, all gates verified `== Operational` with zero gate changes,
  `static_assert` reviewed, construction tests in `building_tests.cpp`.
- Deviations, all load-bearing: the state is appended AFTER `Destroyed`
  (not before `Operational`) so legacy save values 0/1 decode
  unchanged; build times are Base 3 / Depot 2 / Factory 4s — 12s factory
  stalls flipped the soak ladder (Medium beat Hard) twice, shorter
  times preserve it (6957/11111 frames); `constructionTime` is not
  serialized (loaded sites restart from 0 HP); every test sim loop that
  drives `ai.Update` directly now ticks construction exactly like the
  game does (12 files touched).
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — each independent, no shared prerequisites).

---

## Current state (verified fresh)

**Buildings go instantly to full-health `Operational` — no construction phase exists
at all today.**

- `BuildingState` (`src/game/public/economy/Building.h:25-30`): only `Operational`
  and `Destroyed`. No `UnderConstruction`/`Placing` state.
- `Building` struct (`Building.h:32-51`): has `health`/`maxHealth`/`repairCarry` for
  post-placement damage/repair, but **no build-timer, no progress fraction, no
  placement-timestamp field of any kind**.
- `PlaceBuilding` (`src/game/private/economy/Building.cpp:149-170`), lines 156-164:
  ```cpp
  Building building;
  building.type = type;
  building.state = BuildingState::Operational;   // immediately Operational
  building.teamID = teamID;
  building.tileX = tileX;
  building.tileY = tileY;
  building.health = BuildingMaxHealth(type);      // immediately full health
  building.maxHealth = building.health;
  ```
  A placed building is `Operational` at 100% health the same frame it's placed.
  `CanPlaceBuilding` (`Building.cpp:100-122`) only validates footprint/terrain/node
  overlap — zero time/progress concept.
- Placement call sites: `Game.cpp:1299` and `Game.cpp:1317`.
- Building draw loop: `Game.cpp:1931-1962` — draws the rect/sprite plus a selection
  ring; no construction-progress overlay exists since there's no state to visualize.
- No `BuildTime(BuildingType)` exists (units have `BuildTime(UnitType)` at
  `src/game/private/economy/Production.cpp:6-11`; buildings have no equivalent).

### Existing progress-bar pattern to mirror

`Game.cpp:2230-2236` (production queue HUD bar, screen-space, outside
`BeginMode2D`/`EndMode2D`):
```cpp
if (!queue.Empty())
{
    DrawRectangle(ppx, screenHeight - 202, 150, 12, LIGHTGRAY);
    DrawRectangle(ppx, screenHeight - 202, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
}
```
Background-then-foreground two-rectangle technique, foreground width = `barWidth *
fraction`. `ProductionQueue::HeadProgress()` (`Production.h:43`, impl
`Production.cpp:113-121`) = `head.progress / head.buildTime`. For a **world-space**
bar over a building, the existing unit health-bar draw (`Game.cpp:2073-2078`) is the
closer precedent — same two-rectangle technique, but positioned in world space
relative to a footprint rect instead of a fixed HUD position.

## Design

1. **New state**: add `BuildingState::UnderConstruction` (before `Operational` in the
   enum, keeping `Destroyed`/`Count` positions stable per the existing save-compat
   convention documented on this enum), plus two new `Building` fields:
   `float constructionTime = 0.0f;` (elapsed) and a per-type
   `BuildingBuildTime(BuildingType)` function (mirror `Production.cpp:6-11`'s
   `BuildTime(UnitType)` shape exactly).
2. **`PlaceBuilding` changes**: set `state = BuildingState::UnderConstruction`,
   `health` starts at a small fraction (or 0) of `BuildingMaxHealth(type)` rather
   than full — ramping visible health up as construction completes reads clearly as
   "still being built," matching how many RTS games visualize this.
3. **New per-frame update**, e.g. `UpdateBuildingConstruction(Registry&, float dt)`
   called once per frame alongside other building updates (check where
   `UpdateBuildingAutoRepair` is currently called from and add this beside it):
   advance `constructionTime`, ramp `health` proportionally, and flip to
   `BuildingState::Operational` (setting `health = maxHealth` exactly) on completion.
4. **Free correctness win**: every place that already gates on `state ==
   BuildingState::Operational` — base income (`Building.cpp:190`), factory panel
   (`Game.cpp:2214-2220`), attack-target validity (`Building.cpp:217`) — will
   automatically exclude an `UnderConstruction` building with zero changes, since
   they check equality against `Operational` already. Verify each of these three
   sites during implementation rather than assuming, but expect no changes needed.
5. **Visual**: in the building draw loop (`Game.cpp:1931-1962`), when `state ==
   UnderConstruction`, draw a small world-space progress bar (background/foreground
   rectangle pair, same technique as `Game.cpp:2073-2078`'s health bar) above the
   footprint using `constructionTime / BuildingBuildTime(type)` as the fraction.
   Consider also dimming/desaturating the building sprite itself while under
   construction (a cheap `Fade(tint, 0.6f)` on the draw call) as a secondary visual
   cue, but the progress bar is the primary requirement.

## Tests

Extend `building_tests.cpp`: placing a building starts it `UnderConstruction` at
reduced health; `UpdateBuildingConstruction` advances `constructionTime` and
`health` proportionally each call; completion flips to `Operational` with `health ==
maxHealth` exactly (not overshooting); an `UnderConstruction` building is excluded
from base income and cannot be attack-targeted or selected for production (reuse
existing tests for those three gates, parametrized over the new state, rather than
writing new gate logic — confirm each already-passing test still passes and add one
new case per gate with a building explicitly left `UnderConstruction`).
