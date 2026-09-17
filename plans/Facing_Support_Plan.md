# Plan: Facing Support (directional sprites)

## Implementation status: PROPOSED (2026-09-17) — not yet implemented.

The prototype sheets are directional: 8 columns = top, top-left, left,
bottom-left, bottom, bottom-right, right, top-right. The run sheet is
8 columns × 4 frames (each column is one direction's cycle); the idle
sheet is 8 columns × 1 row (8 directional poses). Today the engine is
direction-blind: `infantry_walk` plays column 6 while every travel
direction renders the same frames, and row 0 of the sheet is frame-0 of
all 8 facings (an easy wiring mistake — now documented so it isn't
repeated). This plan adds facing end-to-end:
sim stores it, the driver maintains it, the atlas lookup resolves it.

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) for conventions (pure
logic unit-tested, draw calls verified visually) before starting.

---

## Current state (verified fresh)

- **No facing field anywhere.** `Unit` (`Unit.h:103-...`) has `position`,
  `velocity` (direction × speed while stepping, zeroed on every stop —
  `Unit.cpp:1122` waypoint leg, `Unit.cpp:1156` straight-line leg, ~10
  zeroing sites), `state`, `phase` — nothing recording orientation.
- **Atlas lookup is direction-blind.** `Art::UnitSprite(type, moving, id,
  timeSeconds)` (`Art.cpp:404-444`): walk resolves the single anim
  `<prefix>_walk`; idle resolves the single sprite `<prefix>_idle_0_0`;
  both fall back to empty (flat-PNG/rectangle paths take over). No other
  animation-name construction exists.
- **Callers of `UnitSprite` (all must change):** `Game.cpp:2025`
  (render loop), `Game.cpp:45` (selection-box atlas check — facing
  irrelevant, pass anything), `unit_editor.cpp:168` (preview),
  `art_tests.cpp` (10 assertions).
- **Atlas data already has every cell.** Run grid is 4 rows × 8 cols
  (`infantry_walk_<r>_<c>`); idle grid is 1 row × 8 cols
  (`infantry_idle_0_<c>`). Only the *animations* are direction-poor:
  one `infantry_walk` (column 6) + one single-frame `infantry_idle`.
- **Save policy precedent.** `isSelected` IS saved
  (`SaveGame.cpp:83/165`); `controlGroups` is NOT (defaults to 0 on
  load). Facing follows the `controlGroups` precedent: transient visual
  state, not saved, defaults to Right on load (pixel-identical to today).

---

## Design

### 1. `Facing` enum + `FacingFromVelocity` (`Unit.h` / `Unit.cpp`)

```cpp
// 8-way facing = prototype sheet column: 0 top, 1 top-left, 2 left,
// 3 bottom-left, 4 bottom, 5 bottom-right, 6 right, 7 top-right.
// Stored on Unit, maintained by the movement driver; render-only
// consumers (atlas lookup, editor preview) read it, nothing else.
enum class Facing : int
{
    Top = 0,
    TopLeft = 1,
    Left = 2,
    BottomLeft = 3,
    Bottom = 4,
    BottomRight = 5,
    Right = 6,
    TopRight = 7,
    Count
};

// Screen-space velocity (y down) -> nearest octant column. Zero vector
// returns Right (deterministic; callers only invoke it while stepping).
Facing FacingFromVelocity(Vector2 velocity);
```

Mapping (clockwise from East in y-down space): E→6, SE→5, S→4, SW→3,
W→2, NW→1, N→0, NE→7, i.e. `col = (6 - round(deg/45) + 8) % 8` with
`deg = atan2(dy, dx)` in degrees. Pure, needs `<cmath>` (check whether
`Unit.cpp` already includes it before adding).

### 2. Store + maintain on `Unit`

- Field: `Facing facing = Facing::Right;` next to `state` (`Unit.h`) —
  default matches today's always-right rendering exactly.
- Maintain: call `unit.facing = FacingFromVelocity(unit.velocity)` at
  both nonzero-velocity assignments (`Unit.cpp:1122`, `Unit.cpp:1156`),
  guarded on nonzero speed (both sites already know the step succeeded;
  arrival/zeroing sites leave the last facing intact — idle units keep
  facing their travel direction, which is the desired look).
- Do NOT save it (`SaveGame.cpp` untouched — defaults to Right on load,
  same as `controlGroups` defaulting to 0).
- Out of scope v1: attack facing (units keep travel facing while
  attacking; chasing re-faces via the same velocity path anyway).

### 3. Directional atlas lookup (`Art.h` / `Art.cpp`)

```cpp
std::string UnitSprite(UnitType type, bool moving, unsigned int id,
                       float timeSeconds, int facingDir);
```

- `facingDir` is a sheet column 0-7 (plain int, clamped into range —
  keeps `Art` decoupled from the sim's `Facing` enum).
- Walk: try `<prefix>_walk_<dir>` first, fall back to legacy
  `<prefix>_walk` (covers any single-anim sheets, including today's
  data mid-migration), then the idle path as today.
- Idle: try `<prefix>_idle_0_<dir>`, fall back to `<prefix>_idle_0_0`.
  (Assumes the 1-row idle layout today's sheets have; the fallback
  keeps it honest for anything else.)
- Timing/desync math unchanged (per-anim totals, `id * 37` offset).
- Update all callers: render loop passes
  `static_cast<int>(unit.facing)`, the selection-box check passes 0,
  the editor preview passes its new facing selector (§5), tests pass
  explicit columns.

### 4. Atlas data (`data/configs/animations.json` only — grids unchanged)

- Replace the single `infantry_walk` with 8 anims `infantry_walk_0`
  … `infantry_walk_7` (ids 10-17), each 4 frames @120ms from its
  column: dir `c` → sprites `100+c, 108+c, 116+c, 124+c`.
- DELETE the single-frame `infantry_idle` anim (dead once idle resolves
  sprite-direct per direction; nothing references anim id 0 — verify
  with a grep for `FindAnimByName.*"idle"` / anim-id-0 uses before
  deleting).
- Column map (top … top-right for cols 0-7) lives in the `Facing`
  comment + this plan; JSON can't carry comments, and the names are
  self-describing (`infantry_walk_6` = the column-6 cycle).

### 5. Editor preview facing selector (`tools/unit_editor/`)

- Add an `int previewFacing = 6;` to `EditorState` + a `GuiValueBox`
  (0-7) next to "Animate preview" in the sprite tab; pass it to the
  `UnitSprite` call. Draw-call UI, no unit test (verify visually).

---

## Tests

- `art_tests.cpp`: update the 10 `UnitSprite` calls to the new
  signature; keep the existing timing assertions against the dir-6
  anim (same frames, new names); add directional cases (dir 0 walk →
  `infantry_walk_0_0`-column names, i.e. sprites 100/108/116/124;
  dir 3 idle → `infantry_idle_0_3`; unknown dir clamps; type without
  atlas still empty).
- `spritedata_tests.cpp` real-file block: 8 walk anims (not 1+idle);
  assert dir-6 frames unchanged (106/114/122/130) + one more column
  (e.g. dir-0: 100/108/116/124); idle anim entry gone.
- New pure coverage (extend `art_tests.cpp` or `movement_tests.cpp`):
  `FacingFromVelocity` all 8 octants (diagonal-exact + off-axis),
  boundaries (±22.5°), zero vector → Right.
- New `Facing` field: no save test needed (deliberately unsaved);
  driver maintenance covered implicitly by movement tests (velocity
  nonzero → facing matches octant — assert on one leg if cheap,
  else rely on the pure-function tests + visual check).
- Full suite + both soaks afterward (touches `Unit.h` layout and the
  per-frame render path — same discipline as every other change).

## Explicit non-goals (do not build)

- Attack facing / turret-vs-hull split.
- Facing for flat-PNG or rectangle tiers (directionless art stays
  directionless).
- Per-direction walk *timings* (all columns share 4×120ms; tune later
  from the prototype level).
- Migrating other unit types to directional sheets (their lookups hit
  the legacy fallbacks and render exactly as today).
