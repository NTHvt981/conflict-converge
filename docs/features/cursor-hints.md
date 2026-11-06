# Cursor Hints

Shipped: 2026-11-06 (commits `166dd3f`, `33ac981`; generalized the same day).
No deviations from the issue design beyond the RmlUi-only scope.

A cursor-following hint for RmlUi screens: hovering an element whose id has a
hint entry shows that text next to the cursor. First consumer is the Skirmish
setup screen, where the disabled `Start match` says "select a map first"
while no map is selected and never draws once one is (issue #79, from the
first smoke playtest decision `retune`).

## Behavior

- `CursorHint` (per screen) owns mechanics only: on `Update`, when the
  screen's gate is true, it walks the RmlUi hover chain up to the bound
  document, matches an entry id with non-empty text, shows it in the
  document's `#cursor-hint` element, and anchors it cursor + offset
  (14, 22 px), clamped inside the window by a width/height reserve.
- The screen owns policy: it fills a `CursorHintEntry` table and sets text
  through `SetText` in its refresh path. An empty text hides the hint, so
  "only while disabled" is text computed per refresh (`CanStart()`).
- Gate and hover chain are independent: a modal confirm over the screen (or
  the screen being gone) hides the hint even with text present.
- `#cursor-hint` is absolute, `pointer-events: none`, 18dp font in a white
  1dp-bordered box; the run of the start-hint feature showed no layout shift
  (`docs/playtest/2026-11-06-skirmish-setup-hint.md`).
- Placement uses `px`, which maps 1:1 to RmlUi context coordinates while
  mouse input is pumped in raw raylib pixels, so the hint tracks the cursor
  at any uiScale; everything else in the themes uses `dp`.

## Key files

- `src/game/public/app/ui/CursorHint.h` + private `CursorHint.cpp` —
  `CursorHintEntry`, `Bind`/`SetText`/`Update`.
- `src/game/private/app/ui/RmlUiMenus.cpp` — bind in `Init`, per-frame
  `Update` in `Draw` (gate: setup state, no confirm modal).
- `src/game/private/app/ui/RmlUiMenusDialogs.cpp` — `RefreshSetup` sets the
  `BtnStart` entry text from `CanStart()`.
- `data/ui/skirmish_setup.rml` — the `#cursor-hint` element
  (`UiIds::CursorHint`); any participating document adds its own.
- `data/ui/theme.rcss` — `#cursor-hint` style.
- Visual-only by design (no tests), like the raygui tooltip half.

## Decisions

- Cursor-anchored over a static label beside the button: the hint answers
  the hover moment ("why can't I click"), costs no space after selection,
  and shifts no layout. Accepted risk: discoverability depends on resting
  the cursor on the disabled button.
- Screen-owned policy vs helper-owned mechanics: the helper never asks
  `CanStart()` itself; the entry text simply goes empty, so other screens
  reuse it without the setup screen's rules leaking into `app/ui`.
- RmlUi-only by scope: menus are the RmlUi branch, so there is no raygui
  surface for this screen to keep in parity; raygui screens keep their own
  tooltips (`docs/features/hover-tooltips.md`).
