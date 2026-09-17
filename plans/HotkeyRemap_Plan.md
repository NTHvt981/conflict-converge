# Plan: Fully Remappable Hotkeys

## Implementation status: SHIPPED (2026-09-17, commit `42c3556`)

- Shipped with three corrections to this plan:
  1. The table holds **27** actions, not 23 (the F-slot chords were
     undercounted, plus the later AreaRepair toggle) — `hotkey_tests`
     pins the count.
  2. `Bind` does NOT cleanly overwrite on re-run (stale keys survive),
     so `BindShortcuts()` calls `Clear()` first — the plan's "very
     likely safe" assumption was wrong.
  3. The remap screen is a shared `DrawHotkeyRemap(cx)` called from both
     the Settings chain (pre-match) and the paused overlay (in-match);
     the single-screen sketch only worked pre-match. Sim stays frozen
     (gated on `Playing`).
- UI as designed: arming, `GetKeyPressed` capture (Esc/modifiers
  excluded), second-click conflict steal, `hotkey.<action>` persistence.
- Test deviation: the re-run check is a headless registry-level
  equivalent — a full `Game` isn't drivable without a window.

The last unshipped item (of two) from `plans/QoL_Meta_Accessibility_Plan.md` §1. Re-grounded
against the current code — the keybinding surface has grown substantially since that doc's
original sketch (control groups, area-build, attack-ground mode, auto-retreat toggle, replay
viewer, map editor, etc. all added new bindings), so this plan re-derives the full current
inventory and, importantly, identifies a **new architectural wrinkle**: not every binding today
goes through `ShortcutRegistry` — several go through raw `IsKeyDown`/`IsKeyPressed` polling
instead, and those are meaningfully harder to remap.

Read [QoL_Roadmap_Plan.md](QoL_Roadmap_Plan.md) before starting.

---

## Current state (verified fresh)

### `ShortcutRegistry` — unchanged shape, still no Ctrl support
`Shortcuts.h`/`Shortcuts.cpp`: `unordered_map<int, KeyBindings> bindings_`, `KeyBindings{Action
plain; Action chord; bool hasPlain; bool hasChord;}` — a strictly binary plain/Shift-chord model,
keyed by raw raylib key code. `PollAndFire()` checks `IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(
KEY_RIGHT_SHIFT)` for the chord decision. **No Ctrl awareness anywhere in the registry** — Ctrl
support for control groups was built entirely outside it (see below).

### Tier 1: bindings that go through `Game::BindShortcuts()` (straightforward to remap)
Full current inventory, `Game.cpp:274-584`:

| Key | Action |
|---|---|
| F1 | Toggle shortcut-hint overlay |
| P | Toggle pause |
| F5 / F9 | Quicksave / Quickload |
| F6/F7/F8, Shift+F6/F7/F8 | Save/Load named slots 1-3 |
| A | Squad attack-move (Shift = queue) |
| H / G | Stance Hold / Guard |
| V | Squad patrol (Shift = queue) |
| R | Toggle rally-point placement |
| C | Select-all-of-type |
| F | Select-all-Factories |
| B | Toggle move-at-slowest-speed |
| Z | Toggle area-build placement mode |
| X | Toggle attack-ground mode (one-shot, next right-click) |
| T | Toggle auto-retreat on selection |
| J | Jump camera to latest ping |
| LEFT / RIGHT | Replay viewer: step back/forward |
| ESCAPE | Context-sensitive back/deselect/cancel |
| SPACE | Halt all selected units |

Every one of these is a `Bind`/`BindChord` call with a literal `KEY_*` argument and a `[&]` lambda
— exactly the shape the original plan assumed, and exactly as cheap to make data-driven (wrap the
literal argument in a lookup, don't touch the lambda body).

### Tier 2: bindings that do NOT go through `ShortcutRegistry` at all (harder to remap)
Discovered since the original plan — these use raw `IsKeyDown`/`IsKeyPressed` polling directly in
`Game::Update()`, specifically because they need modifier combinations `ShortcutRegistry`'s binary
plain/Shift-chord model can't express:
- **Control groups** (`Game.cpp:1252-1283`): digit keys `1-0`. Plain = recall, Ctrl+digit =
  assign, Shift+digit = add, Ctrl+Shift+digit = set auto-add group. Guarded off while
  `placingType.has_value()` (digits are stolen by the area-build type-picker in that mode).
- **Area-build type picker** (`Game.cpp:1286-1300`): digits `1/2/3` pick Base/ResourceDepot/Factory
  while `placingType` is active.
- **Map editor brush picker** (`Game.cpp:832-841`): digits `1-9` pick terrain brush, only reachable
  in `MenuState::MapEditor`.
- **Alt+right-drag** (line formation): `altDown` computed via raw `IsKeyDown(KEY_LEFT_ALT) ||
  IsKeyDown(KEY_RIGHT_ALT)` at `Game.cpp:1047`, gating several branches of the right-button
  dispatch chain.

Remapping these means replacing the raw `KEY_ONE`..`KEY_ZERO`/`KEY_LEFT_ALT` literals at each poll
site with the same kind of lookup Tier 1 gets — mechanically similar, but there are more call
sites per "action" (e.g. control groups touch 4 distinct modifier combinations across one loop)
and lower value to remap (numbers-for-groups and Alt-for-modifier are fairly standard/expected,
unlike mnemonic letters someone might want to rearrange).

### `MenuSettings` + save/load — unchanged flat format; exact template to copy
`MenuSettings` (`Menu.h:34-46`): `cameraSpeed`, `showMinimap`, `rightDragPan`, `masterVolume`,
`musicVolume`, `sfxVolume`, `mute`. No hotkey field exists yet. `SaveSettings`/`LoadSettings`
(`Menu.cpp:106-203`) remain the same flat `key=value` text format, tolerant of unknown/malformed
lines (confirmed unchanged). **`rightDragPan`'s end-to-end wiring is the exact template to
copy** for a new persisted setting:
- Field: `Menu.h:40`.
- Save line: `Menu.cpp:116`.
- Load block: `Menu.cpp:165-170`.
- UI checkbox, in **both** the Settings screen (`Game.cpp:742-743`) and the in-match Paused
  overlay (`Game.cpp:1905-1906`) — this project duplicates settings UI across those two screens,
  so a new hotkey-remap screen/section needs the same duplication (or, better, factor a shared
  draw function both screens call, if that's cleaner — check whether `rightDragPan`'s checkbox
  code was actually duplicated verbatim or already factored before deciding).

---

## Scope decision: Tier 1 only for v1

Remap the 23 `BindShortcuts()`-registered actions. Explicitly defer Tier 2 (control groups,
area-build/map-editor digit pickers, Alt-modifier) as a documented limitation — lower value, more
call sites per action, and touches input-handling code that's already dense with modifier-gated
branches (control groups' block alone handles 4 modifier combinations in one loop). If a future
pass wants Tier 2 too, it follows the identical `HotkeyMap::KeyFor(name, default)` pattern below,
just applied at each raw poll site instead of at a `Bind()` call.

---

## Design

### `HotkeyMap` (new class, `src/game/public/app/Hotkeys.h`/`.cpp`)
```cpp
class HotkeyMap
{
public:
    int KeyFor(const std::string &action, int defaultKey) const;
    void Rebind(const std::string &action, int newKey);
    std::optional<std::string> ActionForKey(int key) const; // reverse lookup, for conflict warnings
private:
    std::unordered_map<std::string, int> overrides_;
};
```

### `Game::BindShortcuts()` becomes idempotent and re-callable
Wrap every literal `KEY_*` argument:
```cpp
input.shortcuts.Bind(hotkeys.KeyFor("ToggleHints", KEY_F1), [&] { showHints = !showHints; });
```
for all 23 actions — mechanical, no lambda bodies change. After a rebind, `BindShortcuts()` must
re-run in full (unbind everything, rebuild from `hotkeys.KeyFor(...)` again) — confirm
`ShortcutRegistry::Bind` cleanly overwrites an existing map entry for the same key (very likely,
given the `unordered_map<int, KeyBindings>` storage) so re-invoking `BindShortcuts()` after a
rebind is safe without an explicit `Clear()` first; verify this against the current
`Shortcuts.cpp` before assuming it.

### Rebinding UI
A new settings sub-screen (or a scrollable section within the existing Settings screen) listing
all 23 action names with their current key and a "press a key to rebind" affordance per entry.
Conflict handling: `ActionForKey` detects if the chosen key is already claimed by a different
action — warn and require confirmation before implicitly unbinding the other action, rather than
silently creating a dead binding.

### Persistence
Extend `SaveSettings`/`LoadSettings` with one `hotkey.<action>=<key>` line per **overridden**
action only (unconfigured actions stay at their code-level default, keeping the file small) —
raw raylib int key codes, matching the existing convention of storing plain values rather than
symbolic names (no in-game hand-editing UI is implied here since there's now a real remap screen,
so human-readability of the raw file matters less than it would for a `settings.cfg` meant for
manual editing).

---

## Tests

New `tests/unit-tests/hotkey_tests.cpp` (`RunHotkeyTests`): `KeyFor` returns the default when
unconfigured, returns the override after `Rebind`; `ActionForKey` reverse lookup works; a
conflict (two actions claiming the same key after rebinds) is detectable via `ActionForKey` before
committing a second rebind. Extend `menu_tests.cpp`'s save/load roundtrip with `hotkey.` lines,
following its existing malformed-line-rejection test pattern. If time allows, add one integration
check that `BindShortcuts()` re-run after a `Rebind` actually moves the binding (fire the new key
via `ShortcutRegistry::Fire`, confirm the old key no longer fires that action) — this is the one
piece of behavior described above ("re-callable, no stale binding left behind") that's easy to get
subtly wrong and worth a real test rather than just code review.
