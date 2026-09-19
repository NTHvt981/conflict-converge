# Hint Overlay

Shipped: 2026-09-17 (commit `de0bd23`). No deviations.

The F1 shortcut overlay builds Tier-1 lines live from the remap table,
so it always shows current bindings; Tier-2/mouse lines stay literals.

## Behavior

- `ShortcutHintLines(const HotkeyMap&)` preserves order; key names come
  from the pure `HotkeyDisplayName` table (headless-safe — `GetKeyName`
  needs GLFW, so only the remap screen itself uses it).
- Slot ranges join effective keys (`F6/F7/F8 SaveSlot`).
- Tier-1 vs Tier-2 color distinction not implemented (optional).

## Key files

- `src/game/private/app/Hud.cpp` — overlay source.
- `src/game/public/app/Hotkeys.h` — display-name table.
- Tests: `SelectionVisual` suite (defaults + rebind liveness).

## Decisions

- Overlay reads the live map instead of caching strings, so a remap
  shows up without any refresh path.
