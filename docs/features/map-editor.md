# Map Editor

Shipped: 2026-09-17 (commit `e81876f`). No behavior delta from the plan
(code now lives in extracted `MenuScreens`, fields renamed).

In-menu tile map editor behind raygui widgets; the canvas itself stays
custom-drawn.

## Behavior

- 9-way brush picker (`GuiToggleGroup`, number keys write the same var).
- Title/help/status labels; Save-As dialog with traversal-safe name
  guard (`IsValidMapSaveName` rejects empty/traversal).
- Tile paints + marker overlays stay custom canvas code (only chrome
  went raygui; the `GuiGrid` hover simplification was not taken).

## Key files

- `src/game/public/core/MenuScreens.h` + `private/core/MenuScreens.cpp` —
  editor branch, widget wiring.
- `src/game/public/world/MapFile.h` — save-name guard.
- Tests: `MapFile` suite (validation cases).

## Decisions

- Editor validity is name-level only; map-content validation reuses the
  loader's own checks on export.
