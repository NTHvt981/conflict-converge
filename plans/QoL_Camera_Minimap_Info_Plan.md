# Plan: Camera, Minimap & Information QoL

## Implementation status: MOSTLY SHIPPED (updated 2026-09-17)

- Shipped: attack/event pings + camera jump (`cecf442`), right-drag pan
  (`21c6640`, incl. deferred click orders + persisted option).
- NOT shipped: range preview on selection (one draw call — smallest
  remaining item on this roadmap).

Covers, from `missing_features.md` § "Camera, minimap & information":
- Attack/event pings with camera jump (minimap renders, but never alerts)
- Right-drag camera pan option (currently WASD + edge pan only)
- Range preview on placement/selection

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) **P5** (`RightDown()`) and **P7** (no damage-event
dispatch) before starting.

---

## 1. Attack/event pings with camera jump

### What already exists (more than you'd guess)
- **Event dispatcher is production-ready.** `EventDispatcher` (`src/game/public/core/Event.h`,
  `src/game/private/core/Event.cpp`) does `Subscribe`/`Dispatch`/`Clear` over an
  `unordered_map<EventType, vector<Handler>>`, synchronous, in-subscription-order. `Game` owns the
  single instance (`Game.h:71`, `EventDispatcher events;`).
- **`EventType::UnitSpawned`/`UnitDestroyed` are already dispatched** — from
  `UnitFactory::SpawnPrepaid`/`DestroyUnit` (`src/game/private/units/UnitFactory.cpp:45-73`).
  `Game.cpp:107` already subscribes to `UnitSpawned` (currently just plays a sound).
- **`EventType::UnitDamaged` is reserved in the enum but never dispatched** — `Event.h:21-22`'s
  comment says so explicitly: *"combat and gather ticks stay dispatcher-free for now."*
  `Combat.h`/`Combat.cpp`'s `ResolveAttack`/`ResolveBuildingAttack` are plain free functions with no
  `EventDispatcher` awareness at all.
- **`Event` carries no payload** (`Event.h:33-37`: just a `type` field) — every event needs a
  derived struct to carry data (position, team, etc.). The test-only `SpawnedEvent : Event { int
  teamID; }` in `event_tests.cpp` is the proof-of-pattern for how to extend it.
- **Camera-jump math already exists and is exercised**: `Minimap::MinimapToWorld(Vector2
  minimapPx, int mapW, int mapH)` (`src/game/public/app/Minimap.h:31-34`,
  `src/game/private/app/Minimap.cpp:50-72`) is already used for minimap click-to-move
  (`Game.cpp:594-598`: `camera.view.target = minimap.MinimapToWorld(...)`) — a ping's camera-jump
  is literally the same one-line assignment, just triggered by an event instead of a click.
  `Minimap::WorldToMinimap` (`Minimap.h:29`) is the inverse, for drawing a blip on the minimap.
  `Minimap::ViewportRect` (`Minimap.h:40`) tells you whether a world point is already on-screen
  (skip the alert/flash if so).

### Decision: how to detect "damage just landed" without breaking `Combat.h`'s signatures
Two options — **recommend option B** for this pass (lower risk, no signature changes to a
well-tested free-function module):
- **Option A**: thread `EventDispatcher&` into `ResolveAttack`/`ResolveBuildingAttack`. Touches
  every call site (`Unit.cpp`'s `EngageTarget`, `SeparateUnits`'s crush-damage calls, anywhere else
  combat resolves) and `combat_tests.cpp`'s test fixtures, which currently construct bare `Unit`
  pairs with no dispatcher in scope. Higher blast radius.
- **Option B (recommended)**: reuse the existing per-frame poll already in `Game.cpp:709-730` that
  detects "a unit was just hit" via `unit.hitFlashTime > 0.20f` (used today to spawn impact
  particles) — this is already, structurally, a "just got hit" detector with zero new plumbing.
  Add a ping-emission call right next to the existing particle-spawn call in that same loop:
  ```cpp
  registry.Each<Unit>([&](Entity id, const Unit &unit) {
      const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
      if (unit.hitFlashTime > 0.20f)
      {
          art.ParticlesPool().SpawnBurst(center, YELLOW, 6, 90.0f, 0.25f);
          if (unit.teamID == playerTeamID) { pings.Raise(center, PingKind::UnderAttack); }
      }
  });
  ```
  This sidesteps `EventDispatcher` entirely for the damage case — treat it as a separate, simpler
  "ping system" rather than routing everything through `Event`/`EventDispatcher`. Use the real
  `EventDispatcher` (`UnitSpawned`/`UnitDestroyed`) for spawn/death pings, since those already
  dispatch events; use the poll approach only for damage, where no event exists yet and adding one
  is disproportionate to this feature's scope.

### New `Pings` component
New small class, e.g. `src/game/public/app/Pings.h` / `.cpp` (fits the `app` layer alongside
`Minimap`/`Hud`):
```cpp
enum class PingKind { UnderAttack, UnitLost, ... };

struct Ping
{
    Vector2 worldPos;
    PingKind kind;
    float age = 0.0f; // seconds since raised; used to fade minimap blip / flash duration
};

class Pings
{
public:
    void Raise(Vector2 worldPos, PingKind kind);
    void Update(float dt); // ages out old pings, drop after e.g. 5s
    const std::vector<Ping> &Active() const;
private:
    std::vector<Ping> pings_;
    // Optional: per-kind retrigger floor (mirror the existing per-sound
    // retrigger-floor pattern already used elsewhere in this codebase for
    // audio spam prevention — check Audio.h/.cpp for the exact idiom) so a
    // unit taking continuous fire doesn't spam a new ping every frame.
};
```
Subscribe to `UnitDestroyed` in `Game::Init()` (mirroring the existing `UnitSpawned` subscription
at `Game.cpp:107`) — but recall `Event` carries no payload, so `UnitDestroyed`'s handler as it
exists today has no position to ping at. **Add a derived payload struct**:
```cpp
struct UnitLifecycleEvent : Event { Vector2 position; int teamID; };
```
and change `UnitFactory::SpawnPrepaid`/`DestroyUnit` (`UnitFactory.cpp:45-73`) to dispatch this
derived type instead of a bare `Event` — position is already available at both call sites (before
`Dispatch` in `SpawnPrepaid`, since `Add` already ran; before `Destroy` in `DestroyUnit`, since
`Dispatch` happens before `registry_.Destroy`). `EventDispatcher::Subscribe`'s handler signature
needs checking — if it takes `const Event&`, the subscriber `static_cast`s or `dynamic_cast`s to
`UnitLifecycleEvent` (check `event_tests.cpp`'s `SpawnedEvent` usage for the established
cast pattern).

### Minimap blip rendering + click-to-jump
In `Hud.cpp`/`Minimap.cpp`'s minimap draw call, iterate `pings.Active()`, convert each
`worldPos` via `WorldToMinimap`, draw a small pulsing/fading marker (fade by `age`). For "camera
jump," reuse the exact mechanism already at `Game.cpp:594-598` — either let the player click the
minimap where the blip is (already works, no change needed) or add a dedicated "jump to last
ping" hotkey that sets `camera.view.target = <most recent ping's worldPos>` directly.

### Tests
New `tests/unit-tests/ping_tests.cpp` (`RunPingTests`): `Raise`/`Update`/aging/expiry as pure
logic (no rendering). Extend `event_tests.cpp` if the `UnitLifecycleEvent` payload type is added,
verifying `SpawnPrepaid`/`DestroyUnit` dispatch it with correct `position`/`teamID`.

---

## 2. Right-drag camera pan option

### Existing primitives to reuse directly
- `GameCamera::Pan(Vector2 delta)` (`src/game/public/app/GameCamera.h:19-29`,
  `src/game/private/app/GameCamera.cpp`) already does exactly "add a screen-space delta to
  `view.target`" — `UpdateWASD`/`UpdateEdgePan` both already funnel into it. **No new camera math
  needed** — a right-drag pan handler just needs to compute a per-frame mouse delta and call
  `camera.Pan(-delta)` (negate: dragging right should move the view left, standard "grab the
  world" pan convention).
- Drag-state pattern to imitate: `Game.h:100-101`'s `dragging`/`dragStart` +
  `Game.cpp:592-639`'s press/release handling for box-select is the template — but for a
  *continuous* pan (not a start/end rect), you actually want a simpler **per-frame delta from the
  previous frame's mouse position**, not a start/end pair.

### Gap: `RightDown()` doesn't exist yet
Covered in roadmap **P5** — implement it in `InputManager` before this feature (see
[QoL_Selection_And_Control_Plan.md](QoL_Selection_And_Control_Plan.md) §0.2 for the exact
mirror-of-`LeftDown()` diff if it hasn't been done yet by the time you reach this plan).

### Implementation
Add to `Game` (or `InputManager`, if a "previous mouse position" concept belongs there instead —
recommend `InputManager` since it already owns all other raw mouse state):
```cpp
// InputManager.h
Vector2 MouseDeltaScreen() const; // this frame's mouse movement in screen px
```
Computed in `PollLive()`/`Snapshot()` by diffing against the previous frame's stored mouse
position (add a `Vector2 prevMouseScreen_` private field, updated at the end of each
`Snapshot`/`PollLive` call).

In `InputManager::Update` (`InputManager.cpp:3-9`, the single per-frame pump already called from
`Game.cpp:370`), after the existing `camera.UpdateWASD`/`UpdateEdgePan` calls, add:
```cpp
if (RightDown() && !justStartedRightDrag) // see disambiguation note below
{
    camera.Pan(-MouseDeltaScreen() / camera.view.zoom); // divide by zoom so pan feels consistent at any zoom level, matching UpdateWASD's existing zoom-aware speed if it has one — check GameCamera.cpp for whether WASD pan already scales by zoom and mirror that exact convention
}
```

### Disambiguation with existing right-click order behavior
Right-click today *always* issues an order on `RightPressed()` (`Game.cpp:641-667`) — the moment
you start a right-drag-pan gesture, that same press will also fire an order to wherever the button
was first pressed, unless you gate it. Recommended approach, using the same threshold idiom
already established for left-click's click-vs-drag distinction (`Game.cpp:614`'s 6px threshold):
1. Track total drag distance since `RightPressed()` (accumulate `MouseDeltaScreen()` magnitude
   while `RightDown()`).
2. Only invoke `camera.Pan(...)` once the accumulated distance exceeds a small threshold (e.g.
   6-10px, matching the left-click threshold for consistency).
3. On release (`!RightDown()` after having been down), only dispatch the move/attack-move order
   (today's `Game.cpp:641-667` logic) if the accumulated drag distance stayed **under** that same
   threshold — i.e., a "real" right-click (no meaningful drag) still orders exactly as before; a
   right-drag pans and issues no order.

This is the same kind of click-vs-drag disambiguation already proven for left-click at
`Game.cpp:610-627` — mirror its structure exactly, just applied to the right button and to camera
panning instead of box-select.

**This should be an opt-in *option* per the feature's own naming ("right-drag camera pan
option")** — add a `MenuSettings` bool (e.g. `rightDragPan = false`) so players who prefer
right-click-always-orders (today's behavior) aren't surprised by a default behavior change. Persist
it through `SaveSettings`/`LoadSettings` (`Menu.cpp`) following the existing flat `key=value`
format exactly (see [QoL_Meta_Accessibility_Plan.md](QoL_Meta_Accessibility_Plan.md) for more
detail on that format if you need to touch it further for hotkey remapping too — coordinate if
both land around the same time to avoid merge conflicts in `Menu.cpp`).

### Tests
`camera_tests.cpp`: verify `Pan` is called with the expected delta given a scripted sequence of
`MouseDeltaScreen()` values (test `GameCamera` in isolation, no real input). `input_manager_tests.cpp`:
verify `MouseDeltaScreen()` computes correctly across two `Snapshot()` calls with different mouse
positions. The click-vs-drag threshold logic in `Game.cpp` itself is presumably untested today
(it's inline in the render/update loop) — if there's an existing test harness that drives `Game`
end-to-end (check for one before assuming there isn't), extend it; otherwise this piece follows the
project's "draw-call-adjacent glue code is untested" convention and can ship without a dedicated
test, same as the existing left-click drag logic.

---

## 3. Range preview on placement/selection

### Scope correction: "on placement" has no flow to hook into yet
**There is no player-driven building-placement flow in this codebase at all** —
`PlaceBuilding` (`Building.h:60-63`) is only ever called from AI/skirmish setup code
(`AICommander.cpp`, `Skirmish.cpp:169`), never from `Game.cpp` in response to player input. Range
preview "on placement" therefore has nothing to preview onto. **This plan implements range preview
on selection only.** If/when
[QoL_Orders_And_Queueing_Plan.md](QoL_Orders_And_Queueing_Plan.md) §3b's building-placement flow
lands, extend the same technique there (draw a range/reach circle under the placement ghost) as a
small follow-up — don't block this feature on that one.

### Range preview on selection (clean, ready-to-implement hook)
`Unit::attackRange` (`Unit.h:85`) is already a plain int, already circle-based per its own comment
("pixels (circle/radius check, M3G4/M4)"). The per-unit render loop already draws the selection
outline at a known point (`Game.cpp:~999-1007`, `center` already computed a few lines above as
`{ body.x + 16.0f, body.y + 16.0f }`). Add, inside the existing `if (unit.isSelected)` block:
```cpp
if (unit.attackRange > 0)
{
    DrawCircleLinesV(center, static_cast<float>(unit.attackRange), Fade(RED, 0.35f));
}
```
Confirmed available in this raylib build: `DrawCircleLinesV(Vector2, float, Color)`. Stylistically
consistent with existing circle draws in the same file (`DrawCircleV` for resource nodes and the
move-order marker) — this is a pure draw-call addition, no new tests needed per the project's
established convention (draw calls are untested; the underlying data, `attackRange`, is already
covered by unit-stats tests).

### Optional: hover preview (before selecting)
There's no existing "hovered entity" concept — `PickUnitAt(Registry&, Vector2 worldPos)`
(`Selection.h:10`) is currently only called on left-click release. A hover-preview variant would
add a per-frame (render-only, no selection side effect) `PickUnitAt(registry,
input.MouseWorld(camera))` call solely to decide whether to draw a range ring under the cursor.
This is a nice-to-have beyond the base feature — implement the on-selection version first, add
hover as a follow-up only if requested.

### Tests
None required beyond what already exists (`unit_stats_tests.cpp` already covers `attackRange`
values per type) — this is a pure draw-call addition per the project's established
testing/rendering split.
