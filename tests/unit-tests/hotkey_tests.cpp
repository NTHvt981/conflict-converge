// Unit tests for QoL remappable hotkeys (Tier 1): HotkeyMap defaults,
// overrides, reverse lookup, conflict detection, and the Clear+rebuild
// contract Game::BindShortcuts relies on for idempotent re-running.

#include "test_harness.h"

#include "Hotkeys.h"
#include "Shortcuts.h"

void RunHotkeyTests()
{
    // --- table: 27 Tier-1 actions, all known, all with usable defaults ---
    CC_CHECK(NumHotkeyDefs() == 27);
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        CC_CHECK(IsKnownHotkeyAction(kHotkeyDefs[i].action));
        CC_CHECK(kHotkeyDefs[i].defaultKey > 0);
        CC_CHECK(HotkeyDefFor(kHotkeyDefs[i].action) == &kHotkeyDefs[i]);
    }
    CC_CHECK(!IsKnownHotkeyAction("Spaceship"));
    CC_CHECK(HotkeyDefFor("Spaceship") == nullptr);

    // --- KeyFor: default when unconfigured, override after Rebind ---
    HotkeyMap hotkeys;
    CC_CHECK(hotkeys.KeyFor("ToggleHints") == KEY_F1);
    CC_CHECK(hotkeys.KeyFor("LoadSlot1") == KEY_F6); // chord shares the key
    CC_CHECK(hotkeys.KeyFor("Spaceship") == 0);      // unknown: never bound
    CC_CHECK(hotkeys.Overrides().empty());

    hotkeys.Rebind("ToggleHints", KEY_F2);
    CC_CHECK(hotkeys.KeyFor("ToggleHints") == KEY_F2);
    CC_CHECK(hotkeys.Overrides().size() == 1);
    CC_CHECK(hotkeys.Overrides()[0].first == "ToggleHints");
    CC_CHECK(hotkeys.Overrides()[0].second == KEY_F2);

    // --- Rebind ignores unknown actions and non-positive keys ---
    hotkeys.Rebind("Spaceship", KEY_F2);
    hotkeys.Rebind("TogglePause", 0);
    hotkeys.Rebind("TogglePause", -1);
    CC_CHECK(hotkeys.KeyFor("TogglePause") == KEY_P);
    CC_CHECK(hotkeys.Overrides().size() == 1);

    // --- ActionForKey: default owner, then the override wins ---
    CC_CHECK(hotkeys.ActionForKey(KEY_F1).value_or("") == "");
    HotkeyMap fresh;
    CC_CHECK(fresh.ActionForKey(KEY_F1).value_or("") == "ToggleHints");
    CC_CHECK(fresh.ActionForKey(0).has_value() == false);
    fresh.Rebind("ToggleHints", KEY_F2);
    CC_CHECK(fresh.ActionForKey(KEY_F2).value_or("") == "ToggleHints");
    CC_CHECK(fresh.ActionForKey(KEY_F1).has_value() == false); // freed

    // --- shared-key nuance: rebound plain frees the key for its chord twin ---
    HotkeyMap slots;
    CC_CHECK(slots.ActionForKey(KEY_F6).value_or("") == "SaveSlot1"); // table order
    slots.Rebind("SaveSlot1", KEY_F2);
    // F6 still fires the chord twin — rebinding plain never orphans it.
    CC_CHECK(slots.ActionForKey(KEY_F6).value_or("") == "LoadSlot1");

    // --- conflict detectable before committing a second rebind ---
    HotkeyMap conflicts;
    conflicts.Rebind("AttackMove", KEY_G); // G is StanceGuard's default
    CC_CHECK(conflicts.ActionForKey(KEY_G).value_or("") == "AttackMove"); // first wins

    // --- ClearOverrides restores every default ---
    conflicts.ClearOverrides();
    CC_CHECK(conflicts.ActionForKey(KEY_G).value_or("") == "StanceGuard");
    CC_CHECK(conflicts.Overrides().empty());

    // --- the BindShortcuts contract: Clear + rebuild moves a binding ---
    // (headless stand-in for Game::BindShortcuts re-running after Rebind;
    // a full Game isn't drivable without a window, but the mechanism —
    // Clear, then Bind every KeyFor — is exactly what it executes).
    HotkeyMap bound;
    ShortcutRegistry registry;
    int hints = 0;
    int pause = 0;
    registry.Bind(bound.KeyFor("ToggleHints"), [&] { ++hints; });
    registry.Bind(bound.KeyFor("TogglePause"), [&] { ++pause; });
    CC_CHECK(registry.Fire(KEY_F1));
    CC_CHECK(hints == 1);

    bound.Rebind("ToggleHints", KEY_F2);
    registry.Clear();
    registry.Bind(bound.KeyFor("ToggleHints"), [&] { ++hints; });
    registry.Bind(bound.KeyFor("TogglePause"), [&] { ++pause; });
    CC_CHECK(!registry.Fire(KEY_F1)); // stale binding left nothing behind
    CC_CHECK(registry.Fire(KEY_F2));
    CC_CHECK(hints == 2);
    CC_CHECK(registry.Fire(KEY_P));
    CC_CHECK(pause == 1);
}
