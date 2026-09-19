# Accessibility

Shipped: fully shipped 2026-09-17, including the two extracted leftovers
(`HotkeyRemap_Plan.md`, `ColorBlindMode_Plan.md`, commit `42c3556`).

Remappable Tier-1 hotkeys with persistence, color-blind team palette, UI
scale, and remap-aware F1 hints. (UI scale rode along in code though no
plan scoped it.)

## Behavior

- 27 Tier-1 actions remappable (`HotkeyMap`: defaults, `Rebind`,
  reverse lookup, override-only persistence in `settings.cfg`);
  `BindShortcuts` clears and re-binds through the map, so remaps apply
  live. Conflict-steal in the remap screen (Settings + pause).
- Tier-2 inputs (control-group digits, area-build picker, editor brush,
  `Alt`) are raw polling by design — not remappable.
- Color-blind mode: Okabe-Ito orange/sky team tints (off = blue/red),
  applied to atlas sprites, rect fallback, HUD portraits, and minimap
  dots; persisted, live-toggled from Settings and pause.
- UI scale 0.75–2.0 persisted, applied live to raygui text size.
- F1 hint overlay renders through the live map, so it always shows
  current bindings.

## Key files

- `src/game/public/app/Hotkeys.h` + `private/app/Hotkeys.cpp` — 27-entry
  def table, `HotkeyMap`.
- `src/game/private/core/Game.cpp` (`BindShortcuts`) — full Tier-1 bind
  inventory through `KeyFor`.
- `src/game/private/core/MenuScreens.cpp` — remap screen, settings
  checkboxes, editor save.
- `src/game/public/app/Menu.h` + `private/app/Menu.cpp` — `MenuSettings`
  flat persistence (hotkey overrides, color-blind, uiScale).
- `src/game/public/app/Art.h` — instance `TeamTint` + mode flag.
- `src/game/private/app/Hud.cpp` — remap-aware hint lines.
- Tests: `Hotkey` (defs/defaults/rebind/conflicts), `Menu` (round-trip +
  garbage rejection), `Art` (default/CB/revert), `SelectionVisual`
  (hint lines) suites.

## Decisions

- Only-overrides persistence (no full keymap dump in the settings file).
- Shared remap screen for Settings and pause (sim frozen on `Playing`).
- Headless registry-level rebind test instead of a full-`Game` test.
- Open by design: Tier-2 remapping.
