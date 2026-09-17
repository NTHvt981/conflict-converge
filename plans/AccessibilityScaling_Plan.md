# Plan: Accessibility — UI Scale, Text Size, High Contrast

## Implementation status: SHIPPED — Phase 1 only (2026-09-17, commit `ff8ea6a`)

- As designed: `uiScale` in `MenuSettings` (persisted, clamped
  [0.75, 2.0] on load), `GuiSetStyle(DEFAULT, TEXT_SIZE, ...)` at the
  startup + live Settings-slider call sites, roundtrip/clamp tests in
  `menu_tests.cpp`.
- Phase 2 (raw-`DrawText` wrapper) and high contrast remain follow-ups,
  as scoped.
One of several standalone UI/UX polish plans (see the others alongside this one in
`plans/` — independent of them, but follows the exact settings-persistence template
`ColorBlindMode_Plan.md` already established and shipped).

---

## Current state (verified fresh)

### Text drawing is split into two disjoint systems — the key fact this plan turns on

**1. raygui controls** (`GuiLabel`, `GuiButton`, `GuiCheckBox`, `GuiSlider`,
`GuiPanel`, `GuiWindowBox`, etc.) take no font-size argument — they all read one
global style value, `GuiGetStyle(DEFAULT, TEXT_SIZE)`, internally. Confirmed in
`deps/raygui/src/raygui.h`:
- Style enum `TEXT_SIZE` (`raygui.h:663`).
- Default set once: `GuiSetStyle(DEFAULT, TEXT_SIZE, 10);` in
  `GuiLoadStyleDefault()` (`raygui.h:5082`).
- `GuiDrawText`, the shared routine every control calls, reads this value for line
  height/positioning (`raygui.h:5590`); `GuiCheckBox` measures its label the same way
  (`raygui.h:2330-2332`).
- **The game never calls `GuiSetStyle`/`GuiLoadStyle` anywhere** (confirmed via
  repo-wide grep of `src/game/`) — it runs on raygui's compiled-in default the whole
  time. One existing read-only precedent: `Game.cpp:381` (`const int fontSize =
  GuiGetStyle(DEFAULT, TEXT_SIZE);`, used only to measure label width in the hotkey
  remap screen).
- **Implication**: a single `GuiSetStyle(DEFAULT, TEXT_SIZE,
  static_cast<int>(10 * uiScale));` call, made whenever the setting changes (or once
  per frame — it's cheap), scales *every* raygui-drawn label/button/checkbox/slider
  across the whole game for free. This is the cheap win.

**2. Raw raylib `DrawText(...)` calls** take an explicit hardcoded integer literal
per call site, completely independent of raygui's style system — scaling
`DEFAULT/TEXT_SIZE` has zero effect on these. Inventory of hardcoded-size `DrawText`
call sites in `Game.cpp` (line: size): 372:28, 864:52, 865:20, 907:28, 967:28,
1010:28, 1088:24, 1160:16, 1162:14, 1163:14, 1182:14, 1949:24, 2224:16, 2228:16,
2234:16, 2252:14 (shortcut hints, see `InteractiveHintOverlay_Plan.md`), 2269:16,
2271:14 — plus `Hud.cpp:253` (control-group badge, size 10) and several more
`TextFormat`-driven lines without a bare literal visible inline (rally/node/damage
labels). None read from a shared constant; each is independent. A "larger text"
option covering these needs either (a) a `DrawTextScaled`-style wrapper that
multiplies the passed size by `menu.settings.uiScale`, touched into all ~20+ call
sites, or (b) scoping v1 to raygui-only text and leaving raw HUD/world-space text at
fixed size.

### Settings persistence template — confirmed, copy exactly

`MenuSettings` (`Menu.h:35-54`): `colorBlindMode` is a plain `bool` at line 44 with a
comment "Persisted like the other settings; applied live to `art`." A new `float
uiScale = 1.0f;` field follows the same style.

`SaveSettings` (`Menu.cpp:108-129`): flat `key=value`, one line per setting (line
119: `file << "colorBlindMode=" << (settings.colorBlindMode ? 1 : 0) << "\n";`).

`LoadSettings` (`Menu.cpp:131-250`): `if/else if` chain; float settings use
`ParseFloat` + `ClampFloat(number, lo, hi)` (e.g. `cameraSpeed`/`masterVolume`,
lines 158-164/186-192) — `uiScale` follows this exact shape.

**Live-apply pattern to copy** — `colorBlindMode` is applied immediately (not just
on Back/save) at three identical-shape call sites:
- Startup, right after `LoadSettings(...)`: `Game.cpp:151-152`,
  `art.SetColorBlindMode(menu.settings.colorBlindMode);`.
- Settings screen: `Game.cpp:985-987`, checkbox draw then
  `art.SetColorBlindMode(...)`, commented "live, no reopen needed".
- Pause screen: `Game.cpp:2302-2304`, identical.

A `uiScale` setting follows the same three-site pattern, but calls
`GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(10 * menu.settings.uiScale));`
directly instead of an `Art` setter — no new class/setter needed since raygui's
style is process-global state, unlike `Art`'s per-instance `colorBlindMode_` member.

### High contrast — no existing concept to extend

`TeamTint`/`SetColorBlindMode` (`Art.cpp:476-489`) only swaps team colors, never
touches UI chrome/background/text contrast. A high-contrast mode would be new, most
plausibly a `GuiSetStyle` sweep over `BACKGROUND_COLOR`/`BORDER_COLOR_NORMAL`/
`TEXT_COLOR_NORMAL`/`BASE_COLOR_NORMAL` (same style-enum area as `TEXT_SIZE`,
settable the identical way) — architecturally cheap via the same mechanism, but
needs its own persisted bool following the `colorBlindMode` template, and (like
font size) does not reach raw-`DrawText` call sites' hardcoded `Color` literals
(`DARKGRAY`, `GRAY`, `WHITE`, etc.).

## Scope decision: phase this explicitly, don't ship it as one all-or-nothing change

- **Phase 1 (this plan's v1)**: `uiScale` float setting, applied via
  `GuiSetStyle(DEFAULT, TEXT_SIZE, ...)` at the three `colorBlindMode`-style call
  sites. Covers every raygui-driven menu/panel/HUD-button text for free. Ship this
  alone first — it's cheap and covers most player-facing UI chrome.
- **Phase 2 (separate follow-up, don't block Phase 1 on it)**: wrap the ~20+ raw
  `DrawText` call sites for full coverage of in-world/HUD text (damage numbers,
  hints, node labels, etc.). Larger diff, lower risk-adjusted priority than Phase 1.
- **High contrast**: separate feature, same mechanism, own persisted setting — treat
  as an independent follow-up, not bundled into the `uiScale` work.

## Design (Phase 1)

1. Add `float uiScale = 1.0f;` to `MenuSettings` (`Menu.h`, alongside
   `colorBlindMode`).
2. `SaveSettings`: add `file << "uiScale=" << settings.uiScale << "\n";`.
3. `LoadSettings`: add an `else if (key == "uiScale")` branch using
   `ParseFloat`/`ClampFloat(number, 0.75f, 2.0f)` (reasonable min/max — small enough
   to not break layout, large enough to matter for readability).
4. Add a `GuiSlider` for `uiScale` in the Settings screen (same section as
   `cameraSpeed`'s slider) and call `GuiSetStyle(DEFAULT, TEXT_SIZE,
   static_cast<int>(10 * menu.settings.uiScale));` immediately after, live, matching
   the colorblind checkbox's "no reopen needed" comment style.
5. Also set it once at startup (after `LoadSettings`, alongside the existing
   `art.SetColorBlindMode(...)` call at `Game.cpp:151-152`) so a saved non-default
   scale takes effect immediately on launch.

## Tests

Extend `menu_tests.cpp`'s save/load roundtrip the same way `colorBlindMode` is
tested: round-trip a non-default `uiScale`, and a malformed/out-of-range value
(confirm `ClampFloat` clamps rather than crashing or accepting garbage, following
the existing garbage-value-rejection test pattern for other float settings). No
test needed for `GuiSetStyle`'s visual effect itself (draw-call-only).
