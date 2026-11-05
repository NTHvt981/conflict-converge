# Skirmish Setup Start Hint

Shipped: 2026-11-06 (commit `166dd3f`). No deviations from the issue design.

A cursor-anchored hint on the Skirmish setup screen while `Start match` is
disabled (no map selected): hovering the button shows "select a map first"
next to the cursor. Once a map is selected the button enables and the hint
never draws (issue #79, from the first smoke playtest decision `retune`).

## Behavior

- `RmlUiMenus::UpdateStartHint` runs per frame on `SkirmishSetup`: with
  `CanStart()` false and `!ConfirmOpen()`, the hint element shows when the
  context hover element is `#btn-start` or an ancestor-stop walk of it;
  otherwise `display: none`.
- Placement is cursor px + offset (14, 22), clamped inside the window;
  `px` maps 1:1 to RmlUi context coordinates, so the hint follows the OS
  cursor at any uiScale. `#start-hint` is absolute, `pointer-events: none`,
  laid out 18dp font in a white 1dp-bordered box.

## Key files

- `src/game/private/app/ui/RmlUiMenus.cpp` — `UpdateStartHint`, called
  from `Draw` on `SkirmishSetup`.
- `data/ui/skirmish_setup.rml` — `#start-hint` element (`UiIds::StartHint`).
- `data/ui/theme.rcss` — hint style.
- Visual-only by design (no tests), like the raygui tooltip half.

## Decisions

- Cursor-anchored over a static label beside the button: the hint answers
  the hover moment ("why can't I click"), costs no space after selection,
  and shifts no layout. Accepted risk: discoverability depends on resting
  the cursor on the disabled button.
- RmlUi-only by scope: the raygui fallback never draws a SkirmishSetup
  screen, so there is no second surface to keep in parity.
