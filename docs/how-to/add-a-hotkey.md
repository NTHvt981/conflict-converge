# Add a hotkey

## Goal

Wire a new Tier-1 action: a remappable key with a label on the remap
screen, a default binding, and the game behavior it triggers. The remap
screen, the F1 hint overlay, and settings persistence all read the same
definition table — adding one row covers all three.

Raw, non-remappable keys (WASD pan, control-group digits) are Tier-2
and live in the input/selection phases, not this table. This recipe is
Tier-1 only.

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Declare the action.** In `src/game/private/app/input/Hotkeys.cpp`,
   append one row to `kHotkeyDefs`:

   | Field | Use |
   |---|---|
   | `action` | stable id; also the setting key (`hotkey.<action>=<key>`) |
   | `label` | remap-screen display text |
   | `defaultKey` | raylib key code; string names via `HotkeyDisplayName` |
   | `chord` | `true` binds Shift+key through `BindChord` |

   A key shared by a plain and a chord row (the save-slot pair) is
   legal; the chord fires only with Shift held.

2. **Raise the suite's count.** In `tests/unit-tests/hotkey_tests.cpp`,
   bump the `NumHotkeyDefs() == N` check and its comment to the new
   row count.

3. **Bind the behavior.** In `src/game/private/core/ShortcutBindings.cpp`,
   inside `Bind()`, add:

   ```cpp
   input_.shortcuts.Bind(hotkeys_.KeyFor("<action>"), [&] { ... });
   ```

   Gameplay actions must guard themselves the way the existing ones
   do — `worldActive_` gates mid-match, and `menu_.state` gates which
   screen consumes the key. Join flows that fire headlessly through
   `ShortcutRegistry::Fire` are testable without a window.

4. **Check the modal contract.** Every open modal disables shortcuts
   with `ShortcutRegistry::SetEnabled(false)`; a new modal inherits the
   rule from the existing dialog flow, and a binding that must shout
   through a modal is a design change, not a binding flag.
