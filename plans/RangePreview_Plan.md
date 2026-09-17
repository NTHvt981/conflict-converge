# Plan: Range Preview on Selection

## Implementation status: SHIPPED (2026-09-17, commit `42c3556`)

- Exactly as designed: `DrawCircleLinesV` ring in the `isSelected` block
  with the `attackRange > 0` guard (`Fade(RED, 0.35f)`). No deviations.
- Open: visual check (ring color/radius against the selection outline)
  still needs a GPU run — draw calls aren't unit-tested by convention.

The last unshipped item from `plans/QoL_Camera_Minimap_Info_Plan.md` §3. Re-grounded against the
current code — a lot has landed since that doc's original sketch, though this specific feature
area turned out to be untouched by any of it (confirmed below: zero adjacent scaffolding, exactly
as clean a slot as before).

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) for the surrounding conventions before starting.

---

## Current state (verified fresh)

- Per-unit render loop: `Game.cpp`, `registry.Each<Unit>(...)` starting at line **1668**, inside
  the `BeginMode2D`/`EndMode2D` world-space block (`EndMode2D()` at line 1777). `body`
  (`Game.cpp:1675`) and `center` (`Game.cpp:1676`) are computed once near the top of the loop body:
  ```cpp
  const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
  const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
  ```
- The existing `isSelected` block (`Game.cpp:1718-1726`) draws the selection outline and health
  bar:
  ```cpp
  if (unit.isSelected)
  {
      DrawRectangleLinesEx(body, 3.0f, RED);
      const float fraction = UnitHealthFraction(unit);
      DrawRectangle(static_cast<int>(body.x), static_cast<int>(body.y) - 8,
                    static_cast<int>(body.width), 5, Fade(RED, 0.6f));
      DrawRectangle(static_cast<int>(body.x), static_cast<int>(body.y) - 8,
                    static_cast<int>(body.width * fraction), 5, GREEN);
  }
  ```
  Later in the same loop, `controlGroups`, `hitFlashTime`, `state == Attacking`, and
  `hasMoveOrder` each get their own sibling `if` block (`Game.cpp:1727-1755`) — a range-preview
  addition fits this same one-`if`-per-feature style.
- `Unit::attackRange` (`Unit.h:108`, `int attackRange = 0;`) is confirmed **never drawn anywhere**
  today — a full grep across `src/game` for `attackRange` turns up only the field declaration, the
  `UnitStats` copy, combat-range math in `Targeting.cpp`/`Unit.cpp`, and save/load — no
  `DrawCircle*` call references it, including in all the newer ping/repair-panel UI work. Zero
  risk of colliding with something already there.
- Circle-drawing precedent in `Game.cpp`'s render section: only `DrawCircleV` (filled) is used
  today — resource node discs (`Game.cpp:1654`), the move-order marker (`Game.cpp:1754`),
  line-formation drag endpoints (`Game.cpp:1788-1789`), minimap ping blips (`Game.cpp:1812`).
  **No `DrawCircleLinesV`/`DrawRing` (outline) call exists anywhere in the file yet** — a range
  ring needs an outline (a filled disc would obscure the unit and everything behind it), so this
  is a new-but-consistent primitive, not a mismatch with house style. `Fade(...)` is already used
  in this exact loop (`Fade(RED, 0.6f)` at line 1723) for translucency — reuse that idiom for the
  ring's alpha.
- No "hovered unit" concept exists for units (only for building placement — the ghost-preview
  pattern at `Game.cpp:1628-1642` computes `CanPlaceBuilding` every frame unconditionally while
  `placingType` is set, which is the pattern a *hover*-based range preview would need to mirror,
  since there is currently no per-frame `PickUnitAt` call anywhere outside click-edge branches).

---

## Scope decision: selection-based, not hover-based, for v1

Two ways to trigger the preview, both easy given the current code shape:
- **Selection-based** (recommended v1): draw the ring for every currently-selected unit, inside
  the existing `if (unit.isSelected)` block — zero new state, `center`/`attackRange` already in
  scope.
- **Hover-based**: add a new unconditional per-frame `PickUnitAt(registry, input.MouseWorld(camera))`
  call (there isn't one today — this is genuinely new code, not a hookup to something already
  computed), store the result in a new `Game.h` field, and draw the ring for that one entity
  regardless of selection state.

Ship the selection-based version first — it's a one-line change with no new state, matches how
every other per-unit overlay in this loop is already gated (on `isSelected`, on `hitFlashTime`,
etc.), and directly satisfies "range preview on selection" from the feature name. Hover-based
preview is a reasonable follow-up (useful for previewing an enemy's range before engaging) but adds
a new per-frame hit-test and a new field — don't build it speculatively; only add it if
specifically requested after the selection-based version ships.

---

## Implementation

Inside the existing `if (unit.isSelected)` block (`Game.cpp:1718-1726`), add:
```cpp
if (unit.attackRange > 0)
{
    DrawCircleLinesV(center, static_cast<float>(unit.attackRange), Fade(RED, 0.35f));
}
```
That's the entire change. `center` and `unit.attackRange` are both already in scope at that point
in the loop; no new fields, no new functions, no signature changes anywhere.

Two small judgment calls worth deciding explicitly rather than leaving implicit:
- **Color**: `Fade(RED, 0.35f)` matches the existing selection-outline color family in this loop
  (outline and health-bar-background are both `RED`) — consistent, but means the ring can visually
  blend with the selection outline itself at the body's edge for short-range units. If that reads
  poorly once actually rendered, switch to a distinct color (e.g. a translucent `YELLOW` or
  `ORANGE`) — check visually before finalizing, this is a draw-call-only change with no test to
  catch a bad color choice.
- **Zero-range units**: `attackRange == 0` (e.g. non-combat types, if any exist) correctly draws no
  ring via the `> 0` guard — don't remove that guard, a radius-0 circle would still cost a draw
  call for nothing.

---

## Tests

None required — this is a pure draw-call addition, consistent with this project's established
split (pure logic unit-tested, draw calls verified visually). `attackRange`'s underlying values are
already covered by `unit_stats_tests.cpp`. Verify by running the game, selecting a unit, and
confirming the ring appears at the correct radius and stays correctly positioned as the unit moves.
