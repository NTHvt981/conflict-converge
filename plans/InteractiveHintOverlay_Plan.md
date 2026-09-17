# Plan: Live Hotkey Hint Overlay

## Implementation status: SHIPPED (2026-09-17, commit `de0bd23`)

- As designed: `ShortcutHintLines(const HotkeyMap&)` with live Tier-1
  lines + hardcoded Tier-2/mouse lines, order preserved, rebind-liveness
  test in `selection_visual_tests.cpp`.
- One deviation: the plan's `GetKeyName` template crashes headless
  (GLFW-backed, needs an initialized window), so a pure
  `HotkeyDisplayName` table (`Hotkeys.h/.cpp`) renders the key names
  instead — stable across keyboard layouts as a bonus. Defaults match
  the old list except the slot ranges, which now join effective keys
  (`F6/F7/F8 SaveSlot`).
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — each independent, no shared prerequisites).

---

## Current state (verified fresh)

`ShortcutHintLines()` (`src/game/private/app/Hud.cpp:83-117`, declared `Hud.h:23`)
returns a `std::vector<std::string>` of 30 fully hardcoded literal strings (e.g.
`"F1 Hints"`, `"A AttackMove"`, `"F6-8 SaveSlot"`) — no parameters, no access to any
hotkey data. Drawn at `Game.cpp:2247-2254` as a flat static list:
```cpp
if (showHints) {
    const std::vector<std::string> hints = ShortcutHintLines();
    for (std::size_t i = 0; i < hints.size(); ++i)
        DrawText(hints[i].c_str(), 8, 250 + static_cast<int>(i) * 18, 14, Fade(DARKGRAY, 0.8f));
}
```
Pure static text — confirmed exactly matching the "static dump" description this
plan starts from.

**Goes stale after any rebind.** Key names (`"F1"`, `"A"`, `"P"`) are baked into the
literal strings; never read from `HotkeyMap`. Rebinding `AttackMove` off `A` leaves
the overlay printing `"A AttackMove"` regardless.

### Live data source already exists

`HotkeyMap`/`kHotkeyDefs` (`src/game/public/app/Hotkeys.h`): `kHotkeyDefs[]` (extern
array, `Hotkeys.cpp:3-31`), 27 entries of `HotkeyDef{ const char *action; const char
*label; int defaultKey; bool chord; }`. `HotkeyMap::KeyFor(const std::string&) const`
(`Hotkeys.h:34`, impl `Hotkeys.cpp:55-64`) resolves overrides-first-then-default —
exactly the live lookup needed. `Game` owns an instance (`Game.h:107`, `HotkeyMap
hotkeys;`).

**Exact precedent already doing this correctly** — the hotkey remap screen itself,
`Game::DrawHotkeyRemap` (`Game.cpp:368-482`), lines 393-401:
```cpp
for (int i = 0; i < defCount; ++i) {
    const HotkeyDef &def = kHotkeyDefs[i];
    GuiLabel({...}, def.label);
    const int effective = hotkeys.KeyFor(def.action);
    std::string keyText = effective <= 0 ? "-" : GetKeyName(effective);
    if (def.chord) keyText = "Shift+" + keyText;
    ...
}
```
This loop — iterate `kHotkeyDefs`, resolve `hotkeys.KeyFor(action)`,
`GetKeyName()`, prefix `"Shift+"` if chorded — is the ready-made template.

### Important gap to account for, not paper over

Only ~20 of the 30 `ShortcutHintLines()` entries correspond to an actual
`kHotkeyDefs` action (some combined, e.g. `"F6-8 SaveSlot"` covers
`SaveSlot1/2/3`). The rest — `"WASD Camera"`, `"Left Select"`, `"Right Order"`,
`"Alt+RDrag Line"`, `"Shift Queues"`, `"Wheel Zoom"`, `"Ctrl+1-0 Group"`, `"Shift+1-0
AddGrp"`, `"1-0 Recall"`, `"C+S+1-0 AutoAdd"` — describe mouse actions or raw-polled
digit/modifier combos **explicitly outside** `HotkeyMap`/`kHotkeyDefs` (per
`Hotkeys.h:10-13`'s own comment: Tier-2 raw-polled digits/Alt actions are
deliberately deferred from the remap system, see `HotkeyRemap_Plan.md` for that
history). These lines have no live-binding source and must stay hardcoded literals.

## Design

1. **Change `ShortcutHintLines()`'s signature** to take the live data it needs:
   `std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys)` — update
   `Hud.h:23` and the call site (`Game.cpp:2249`, `Game` already has `hotkeys` as a
   member so this is a one-argument addition at the call site).
2. **Split the 30 lines into two groups** inside the function:
   - The ~20 that map to a `kHotkeyDefs` action: generate them dynamically using the
     exact `DrawHotkeyRemap` template above — for each relevant `HotkeyDef`, build
     `"<GetKeyName(hotkeys.KeyFor(action))> <label>"` (or however the label reads
     best; match the existing line phrasing style, e.g. `"F1 Hints"` →
     `"<key> Hints"`).
   - The ~10 Tier-2/mouse-action lines stay hardcoded literals exactly as today —
     don't invent a fake "live" source for data that has none.
3. Preserve line order/grouping as close to the current list as makes sense, so the
   overlay's overall shape doesn't change jarringly — this is a data-source fix, not
   a redesign of what's shown.
4. Optional follow-up (not required for this plan): visually distinguish the "this
   line reflects your current rebind" lines from the fixed Tier-2 lines (e.g. a
   subtly different color), so a player who's rebound something can trust the
   overlay is live without needing to know which lines can and can't be remapped.

## Tests

Extend `hotkey_tests.cpp` or add a small case: call `ShortcutHintLines(hotkeys)` with
a default `HotkeyMap`, confirm it contains the expected default-key strings (e.g.
`"F1 Hints"`); rebind one action via `hotkeys.Rebind(...)`, call again, confirm the
corresponding line now shows the new key instead of the old one. This is a pure
function once `HotkeyMap` is passed in — no rendering dependency, straightforward to
test in isolation.
