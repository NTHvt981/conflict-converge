# Hint Overlay

Shipped: 2026-09-17 (commit `de0bd23`). Two-column stage: 2026-11-04.

The F1 shortcut overlay builds Tier-1 lines live from the remap table,
so it always shows current bindings; Tier-2/mouse lines stay literals.

## Behavior

- `ShortcutHintLines(const HotkeyMap&)` preserves order; key names come
  from the pure `HotkeyDisplayName` table (headless-safe — `GetKeyName`
  needs GLFW, so only the remap screen itself uses it).
- Slot ranges join effective keys (`F6/F7/F8 SaveSlot`).
- `RefreshHints()` splits the list down the middle into `#hint-col-a`
  and `#hint-col-b` and rebuilds only when the joined text changes.
- The stage is a dimmed, pointer-transparent overlay, so it neither
  covers a panel's text nor takes clicks from the world or the HUD.
- Two 290dp columns of 20dp rows fit the smallest dp canvas (640x360 at
  uiScale 2.0) without scrolling; `max-height: 98%` plus scroll is the
  backstop if the list grows again.
- Tier-1 vs Tier-2 color distinction not implemented (optional).

## Key files

- `data/ui/hud.rml` + `data/ui/hud.rcss` — stage, panel, columns.
- `src/game/private/app/ui/Hud.cpp` — hint line source.
- `src/game/private/app/ui/RmlUiHudPanels.cpp` — `RefreshHints` split.
- `src/game/public/app/Hotkeys.h` — display-name table.
- Tests: `SelectionVisual` suite (defaults + rebind liveness),
  `RmlUiHost` suite (document carries both column ids).

## Decisions

- Overlay reads the live map on every refresh; the cache only skips
  rebuilding identical text, so a remap shows up without a reload.
- The overlay keeps every line on screen instead of scrolling: the HUD
  pump leaves the wheel to the game, so a scroll container would have no
  wheel input.
