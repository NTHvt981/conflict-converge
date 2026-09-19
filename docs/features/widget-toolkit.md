# Widget Toolkit (reference)

Verified 2026-09-17: inventory only, no code. Everything below already
exists in the vendored `deps/raylib/src/raygui.h` (the one
`src/thirdparty/raygui_impl.c` compiles — not the decoy copies under
`deps/raylib/examples/` or `deps/rfxgen/`). Check here before
hand-rolling a control: dropdown, tabs, text input, spinners, color
pickers, scroll panels, and modal dialogs are all available.

## In use

`GuiEnable`/`GuiDisable`, `GuiWindowBox`, `GuiPanel`, `GuiLabel`,
`GuiButton`, `GuiToggleGroup`, `GuiCheckBox`, `GuiSlider`,
`GuiListView`, `GuiTextInputBox` (map-editor Save-As), `GuiEnableTooltip`/
`GuiSetTooltip` (tooltips), `GuiSetStyle` (UI scale), `GuiSetFont`.

## Available unused

- Text: `GuiTextBox`, `GuiValueBox`, `GuiValueBoxFloat`, `GuiSpinner`.
- Choice: `GuiComboBox`, `GuiDropdownBox`, `GuiTabBar`, `GuiToggle`,
  `GuiToggleSlider`, `GuiLabelButton`.
- Layout: `GuiScrollPanel`, `GuiGroupBox`, `GuiLine`, `GuiDummyRec`,
  `GuiGrid` (mouse-cell snapping).
- Lists/dialogs: `GuiListViewEx`, `GuiMessageBox`.
- Color: `GuiColorPicker`/`GuiColorPanel` (+HSV), `GuiColorBarAlpha`/
  `GuiColorBarHue`.
- Misc: `GuiLock`/`GuiUnlock`, `GuiSetAlpha`, `GuiSetState`,
  `GuiLoadStyle*`, `GuiDrawIcon`/`GuiIconText`, `GuiGetTextWidth`.

## Call shape to copy

Bounds `Rectangle` + bound state, called once per frame in the draw
branch, value returned/mutated directly (`GuiListView` and
`GuiToggleGroup` call sites are the clearest examples).
