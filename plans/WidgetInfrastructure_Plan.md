# Plan: Missing Basic Widget Types (Dropdown, Tabs, Text Input)

## Implementation status: VERIFIED, no code (2026-09-17)

- Enablement inventory only, as written — no implementation needed.
- Confirmed in practice this session: the HoverTooltips and MapEditor
  plans consumed `GuiEnableTooltip`/`GuiSetTooltip`, `GuiToggleGroup`,
  and `GuiTextInputBox` exactly as inventoried here; all compiled and
  ran with zero new widget engineering.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — this one is an inventory/enablement plan other plans build on, notably
`MapEditorRaygui_Plan.md`'s "Save As" and `HoverTooltips_Plan.md`'s button
tooltips — but none of those are blocked waiting on this plan specifically, since
each cites the exact raygui call it needs directly).

---

## Current state (verified fresh)

Header actually compiled against: `deps/raygui/src/raygui.h` (6460 lines) — confirmed
via `src/thirdparty/raygui_impl.c` including `"raygui.h"` and it being what
`Hud.cpp`/`Game.cpp` include. **Note**: decoy copies exist under
`deps/raylib/examples/*/raygui.h` and `deps/rfxgen/src/external/raygui.h` — not the
ones this project links against; don't edit those by mistake.

### Currently used (7 widget types + 2 global toggles)
`GuiEnable`/`GuiDisable`, `GuiGetStyle`, `GuiWindowBox`, `GuiPanel`, `GuiLabel`,
`GuiButton`, `GuiToggleGroup` (`Game.cpp:937`), `GuiCheckBox`, `GuiSlider`,
`GuiListView` (`Game.cpp:920`).

### Every raygui widget function in the header never called anywhere in `src/`

Directly answering the dropdown/tabs/text-input question — **all three already
exist in the vendored library, unused**:
- **Text input**: `GuiTextBox` (line 848, plain field), `GuiTextInputBox` (line 863,
  modal title+message+field+button dialog).
- **Dropdown/combo**: `GuiComboBox` (line 842), `GuiDropdownBox` (line 844).
- **Tabs**: `GuiTabBar` (line 860), `GuiTabBarEx` (line 861) — raygui's actual name
  is `GuiTabBar`, there is no `GuiTabControl`.
- **Numeric input**: `GuiSpinner` (845), `GuiValueBox` (846), `GuiValueBoxFloat`
  (847).
- **Simpler button variants**: `GuiToggle` (838), `GuiToggleSlider` (840),
  `GuiLabelButton` (837).
- **Layout/containers**: `GuiScrollPanel` (832), `GuiGroupBox` (829), `GuiLine`
  (830), `GuiDummyRec` (854), `GuiGrid` (855, mouse-cell-snapping grid — a candidate
  for `MapEditorRaygui_Plan.md`'s canvas hover math).
- **Lists**: `GuiListViewEx` (859, explicit entries array + focus tracking, an
  alternative to the already-used `GuiListView`).
- **Dialogs**: `GuiMessageBox` (862, modal confirm dialog).
- **Color picker family**: `GuiColorPicker`, `GuiColorPanel`, `GuiColorBarAlpha`,
  `GuiColorBarHue`, `GuiColorPickerHSV`, `GuiColorPanelHSV` (864-869).
- **Global/style/tooltip/icon API**: `GuiLock`/`GuiUnlock`/`GuiIsLocked`,
  `GuiSetAlpha`, `GuiSetState`/`GuiGetState`, `GuiSetFont`/`GuiGetFont`,
  `GuiSetStyle`/`GuiLoadStyle`/`GuiLoadStyleFromMemory`/`GuiLoadStyleDefault`,
  `GuiEnableTooltip`/`GuiDisableTooltip`/`GuiSetTooltip` (used by
  `HoverTooltips_Plan.md`), `GuiIconText`/`GuiSetIconScale`/`GuiGetIcons`/
  `GuiLoadIcons`/`GuiLoadIconsFromMemory`/`GuiDrawIcon`, `GuiGetTextWidth`.

**Bottom line: nothing needs to be hand-built at the widget level.** This "plan" is
really an enablement/inventory document, not new engineering — every gap identified
across the other UI/UX plans (Save-As text input, accessibility's `GuiSetStyle`
sweep, tooltip API) is a call-site wiring task against an already-vendored,
already-correct widget, following the exact pattern the existing `GuiToggleGroup`
(`Game.cpp:937-938`) and `GuiListView` (`Game.cpp:920-921`) call sites demonstrate.

## Design

There's no standalone feature to build here — this document exists so future UI work
doesn't waste time assuming a widget needs to be hand-rolled when it's already
sitting in the vendored header unused. When picking up any future UI task:

1. **Check this document's table first** before writing custom immediate-mode
   drawing + manual hit-testing for a control — dropdown, tabs, text input,
   spinner, color picker, scrollable panel, and modal dialogs are all already
   available.
2. **Call shape to copy**: every raygui widget in this codebase follows the same
   pattern — a `Rectangle` bounds argument, a bound state variable/pointer, called
   once per frame inside the relevant draw branch, returning/mutating the control's
   value directly (see `Game.cpp:920-921` for `GuiListView`, `937-938` for
   `GuiToggleGroup` as the two clearest existing examples).
3. **Known concrete uses already identified by sibling plans** (don't duplicate
   this work when implementing those plans — this section just cross-references
   them): `GuiTextInputBox` for `MapEditorRaygui_Plan.md`'s Save-As;
   `GuiSetTooltip`/`GuiEnableTooltip` for `HoverTooltips_Plan.md`'s button
   tooltips; `GuiSetStyle(DEFAULT, TEXT_SIZE, ...)` for
   `AccessibilityScaling_Plan.md`'s UI text scale.

## Tests

None needed for this document itself (it adds no code). Each consuming plan's own
test section covers its specific usage.
