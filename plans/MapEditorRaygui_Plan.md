# Plan: Unify Map Editor Onto raygui

## Implementation status: SHIPPED (2026-09-17, commit `e81876f`)

- As designed: brush `GuiToggleGroup` (number keys still work — both
  write `editorBrush`), cosmetic `GuiLabel` parity, Save-As via
  `GuiTextInputBox` with an `IsValidMapSaveName` guard (tested, incl.
  traversal rejection), canvas left custom. No deviations.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — related to `WidgetInfrastructure_Plan.md`, which inventories the raygui
widgets this plan uses, but this plan is implementable independently).

---

## Current state (verified fresh)

Location: `menu.state == MenuState::MapEditor` branch, `Game.cpp:1081-1183`.

**Correction to the premise this plan started from**: MapEditor is not 100%
hand-rolled — `GuiButton` (raygui) already draws its Save and Back buttons. The
genuinely hand-rolled parts are the grid canvas (which by nature stays custom — see
below) and the brush "selector" (which has no visual widget at all today, only raw
key polling). Full inventory:

| Element | Lines | Current impl | raygui target |
|---|---|---|---|
| Title/path label | 1088 | `DrawText("Map Editor - data/maps/custom.map", ...)` | `GuiLabel` (cosmetic) |
| Brush selection | 1089-1098 | **No visual widget** — `const char kBrushes[9]`, `IsKeyPressed(kPaletteKeys[i])` sets `editorBrush`; nothing drawn to represent the 9 brushes as clickable | New `GuiToggleGroup` (9-way, same shape as the difficulty picker at `Game.cpp:937-938`) or a row of `GuiButton`s |
| Terrain grid canvas | 1099-1142 | Manual hit-test (`mouse` vs. `kCell`/`kOx`/`kOy` grid math) + manual `DrawRectangle`/`DrawRectangleLinesEx` double loop | **Stays custom** — raygui has no tile-painter widget. `GuiGrid(Rectangle, text, spacing, subdivs, Vector2 *mouseCell)` (`raygui.h:855`, unused anywhere) could replace the manual hover-cell math (1100-1101) but the colored tile fills still need custom `DrawRectangle` |
| Marker letters (I/O/1/2) | 1144-1159 | `DrawText` overlay per spawn/tile | Leave as-is — overlay text, not an interactive widget, low priority |
| Brush/help text | 1160-1163 | 3x `DrawText` | `GuiLabel` (cosmetic parity) |
| **Save button** | 1164-1177 | **Already `GuiButton`** — calls `WriteMapFile(editorMap, "data/maps/custom.map")` unconditionally, hardcoded path at line 1166 | Already raygui; only the missing filename input needs adding |
| Back button | 1178-1181 | Already `GuiButton` | Already raygui |
| Status line | 1182 | `DrawText(editorStatus.c_str(), ...)` | `GuiLabel` |

### Text-input widget for "Save As" — already vendored, unused

`deps/raygui/src/raygui.h`:
- `GuiTextBox(Rectangle, char *text, int textSize, bool editMode)` (line 848) — plain
  inline text field.
- `GuiTextInputBox(Rectangle, const char *title, const char *message, char *text,
  int textSize, const char *btnText, int *btnActive, bool *secretViewActive)` (line
  863) — a **ready-made modal dialog** (title + message + text field + button)
  bundling everything a "Save As" prompt needs, including modal chrome and a confirm
  button. Both confirmed unused anywhere in `src/`.

## Design

1. **Brush selector**: replace the raw `IsKeyPressed(kPaletteKeys[i])` polling loop
   (1089-1098) with a `GuiToggleGroup` bound to `editorBrush`, laid out as a row/grid
   near the canvas, following the exact call shape already used for the difficulty
   picker (`Game.cpp:937-938`). Keep the number-key shortcuts working too if desired
   (poll both; `GuiToggleGroup`'s active index and the key-driven index both just
   assign into the same `editorBrush` variable — no conflict).
2. **Cosmetic labels**: convert the title, help text, and status line `DrawText`
   calls (1088, 1160-1163, 1182) to `GuiLabel` for visual consistency with every
   other menu screen. Purely mechanical, no behavior change.
3. **Save As**: add a `std::string editorSaveName` (or fixed `char[64]` buffer, since
   `GuiTextInputBox` wants a raw `char*`) field to `Game.h` alongside the existing
   `editorMap`/`editorBrush`/`editorStatus` fields. Add a second button ("Save As...")
   that opens a `GuiTextInputBox` pre-filled with the current filename; on confirm,
   call `WriteMapFile(editorMap, "data/maps/" + editorSaveName + ".map")` instead of
   the hardcoded literal at line 1166. Keep the existing plain "Save" button
   behavior (save to the last-used name, defaulting to `custom.map`) for the
   no-prompt fast path.
4. **Leave the canvas alone** beyond the optional `GuiGrid`-based hover-cell
   simplification — it's correctly custom by nature (no raygui widget paints
   arbitrary colored tiles), don't force it into a widget that doesn't fit.

## Tests

`WriteMapFile`'s round-trip behavior is already covered per
`HotkeyRemap_Plan.md`-style precedent in `mapfile_tests.cpp` — extend it with a case
that writes to a name derived from a "Save As" input string (sanitize/validate the
filename before use, e.g. reject empty or path-traversal-bearing input like `../`,
and test that rejection explicitly). UI wiring itself (`GuiToggleGroup` for brush,
`GuiTextInputBox` for save-as) is draw-call/interaction code — verify manually when
the game is run, per this project's established pure-logic-vs-draw-call testing
split.
