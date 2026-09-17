# Plan: Hover Tooltips

## Implementation status: SHIPPED (2026-09-17, commit `5f1970a`)

- As designed: `GuiEnableTooltip()` at startup, `GuiSetTooltip` on the
  HUD controls, custom world-space unit tooltip (`PickUnitAt` +
  `SelectionSummary` + stance/state line) with 0.4s debounce
  (`UpdateHoverTooltip`, tested), suppressed during drag/order/
  placement gestures.
- Two corrections: `Menu.cpp` owns zero widgets (all menu drawing lives
  in `Game.cpp`), so the menu half landed on the Settings screen
  instead; enemy tooltips additionally gate on live fog so hovering
  can't scout through the shroud.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — each independent, no shared prerequisites). Related to but independent of
`ContextCursors_Plan.md` — both use `PickUnitAt` for per-frame hover, but neither
depends on the other shipping first.

---

## Current state (verified fresh)

Confirmed zero tooltip code: a repo-wide grep for `tooltip` (case-insensitive)
returns no hits in `src/`.

### (a) Existing hover-detection utility, already safe to call every frame

`PickUnitAt(Registry&, Vector2 worldPos)` (`src/game/public/app/Selection.h:16`, impl
`src/game/private/app/Selection.cpp:7-23`) is a pure query — no side effects, O(n)
linear scan testing `worldPos` against each unit's 64x64 tile box. Already called
outside a strict click context (`Game.cpp:1453`, evaluated every time the
right-click-dispatch lambda runs) and at click-time (`Game.cpp:1243`). No team filter
is applied, so it works unmodified for hovering enemy units too. This is the correct
building block — no other "hover a world-space entity continuously" utility exists.

### (b) Data and display-name mapping already available

`Unit` fields worth surfacing (`src/game/public/units/Unit.h:126-222`): `health`
(128), `armorType`/`damageType` (129-130), `attackPower`/`attackRange` (131-132),
`cooldownTime` (134), `speed` (140), `sightRange` (141), `teamID` (156), `type`
(157), `state` (158: Idle/Moving/Attacking), `stance` (180: Hold/Guard/Patrol).

**Display-name mapping already exists and is exactly reusable**:
`UnitTypeName(UnitType)` (`src/game/public/app/Hud.h:17`, impl
`src/game/private/app/Hud.cpp:14-36`) returns "Infantry", "Anti-Armor", "Engineer",
"IFV", "Artillery", "Light Tank", "Heavy Tank", "Prototype Infantry". Already used by
`SelectionSummary(const Unit&)` (`Hud.cpp:59-66`), which formats `"%s  HP %.0f/%.0f
Team %d"` — a ready-made template for tooltip text; a tooltip can call
`SelectionSummary` verbatim or build a richer multi-line version from the same
fields. **Don't use** the differently-purposed lowercase name table in
`UnitConfig.cpp:79-118` (explicitly commented there as being for asset-file lookup,
not display).

### (c) raygui tooltip support — natively implemented, currently never invoked

Vendored at `deps/raygui/src/raygui.h`:
- `GuiEnableTooltip()`/`GuiDisableTooltip()`/`GuiSetTooltip(const char*)`
  (`raygui.h:807-810`, impl `raygui.h:4600-4607`).
- `GuiTooltip(Rectangle)` (`raygui.h:1620`, impl `raygui.h:5803-5825`) draws the last
  string set via `GuiSetTooltip` beneath a control's bounds whenever that control's
  internal state is `STATE_FOCUSED` (natively hovered) and the tooltip system is
  enabled — already wired into control drawing for buttons/toggles/etc.
  (`raygui.h:2058, 2143, 3029`).
- **Nothing in the project calls any of these** (confirmed via the zero-hit grep) —
  hooking up raygui-control tooltips is purely: call `GuiEnableTooltip()` once at
  startup, then `GuiSetTooltip("...")` immediately before each `GuiButton(...)` call
  in `Hud.cpp` (direct raw calls at lines 199, 212, 291, 302, 310 — no wrapper
  function exists, each is its own call site).
- This does **not** apply to world-space unit hover — raygui's tooltip mechanism is
  scoped to its own controls' focus state, not arbitrary world-space hit-tests. Unit
  tooltips need custom rendering (see Design §2).

## Design

### 1. Button/control tooltips (raygui, cheap — do this first)
1. Call `GuiEnableTooltip()` once at startup (near other one-time raygui setup, if
   any exists, or simply near `Game`'s init).
2. Add a `GuiSetTooltip("...")` call immediately before each existing `GuiButton`
   call in `Hud.cpp` (lines 199, 212, 291, 302, 310) and any similarly bare
   `GuiButton`/`GuiCheckBox`/`GuiSlider` calls in `Menu.cpp`'s screens worth
   annotating (Settings sliders, HotkeyRemap entries). Write one short, genuinely
   useful description per control — not a restatement of the visible label.
3. No new widget code needed; this is purely call-site wiring.

### 2. Unit hover tooltip (custom, world-space)
1. Each frame (not just on click), compute `Entity hovered = PickUnitAt(registry,
   input.MouseWorld(camera));` — new call site, patterned after the existing
   evaluate-every-frame usage at `Game.cpp:1453`.
2. If `hovered != kInvalidEntity`, build a short stat block from the `Unit` fields
   listed above (type via `UnitTypeName`, HP, stance, state — reuse
   `SelectionSummary`'s formatting as the baseline, extend with 1-2 more lines if
   useful) and draw it near the cursor: a small `DrawRectangle` background box +
   `DrawText` lines, screen-space (after `EndMode2D()`, alongside other HUD draws),
   positioned via `GetWorldToScreen2D` or simply the current `MouseScreen()` position
   offset by a small margin.
3. Debounce: only show after a short hover delay (e.g. 0.3-0.5s) to avoid tooltip
   flicker while the mouse passes over units during normal play — track a small
   `hoverTime`/`lastHoveredUnit` pair on `Game`, reset when the hovered entity
   changes, only draw once `hoverTime` exceeds the threshold. This mirrors the
   "elapsed time in a state" pattern `MenuTransitionAnimation_Plan.md` also needs —
   implement independently here, don't block on that plan.
4. Should NOT show while actively drag-selecting or issuing an order (check the
   existing drag-select/order-dispatch state flags before drawing, so the tooltip
   doesn't visually collide with an in-progress selection box).

## Tests

Button tooltips are visual-only (per this project's established pure-logic-vs-draw
testing split) — no test needed beyond manual verification. For the unit hover
tooltip's non-visual parts: a small test confirming the hover-delay debounce logic
(hovering a new unit resets the timer; holding over the same unit past the threshold
flips a "should show" bool) can be extracted into a pure function and tested without
rendering, following the pattern of other timer-gated pure-logic tests in this
codebase (e.g. `movement_stall_tests.cpp`'s threshold checks).
