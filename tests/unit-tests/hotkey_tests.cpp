// Unit tests for remappable hotkeys: defaults, overrides, reverse lookup,
// conflict detection, and the Clear+rebuild contract.

#include "test_harness.h"

#include "app/input/Hotkeys.h"
#include "app/input/Shortcuts.h"

void RunHotkeyTests()
{
	CC_CHECK(NumHotkeyDefs() == 27);
	for (int i = 0; i < NumHotkeyDefs(); ++i)
	{
		CC_CHECK(IsKnownHotkeyAction(kHotkeyDefs[i].action));
		CC_CHECK(kHotkeyDefs[i].defaultKey > 0);
		CC_CHECK(HotkeyDefFor(kHotkeyDefs[i].action) == &kHotkeyDefs[i]);
	}
	CC_CHECK(!IsKnownHotkeyAction("Spaceship"));
	CC_CHECK(HotkeyDefFor("Spaceship") == nullptr);

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

	hotkeys.Rebind("Spaceship", KEY_F2);
	hotkeys.Rebind("TogglePause", 0);
	hotkeys.Rebind("TogglePause", -1);
	CC_CHECK(hotkeys.KeyFor("TogglePause") == KEY_P);
	CC_CHECK(hotkeys.Overrides().size() == 1);

	CC_CHECK(hotkeys.ActionForKey(KEY_F1).value_or("") == "");
	HotkeyMap fresh;
	CC_CHECK(fresh.ActionForKey(KEY_F1).value_or("") == "ToggleHints");
	CC_CHECK(fresh.ActionForKey(0).has_value() == false);
	fresh.Rebind("ToggleHints", KEY_F2);
	CC_CHECK(fresh.ActionForKey(KEY_F2).value_or("") == "ToggleHints");
	CC_CHECK(fresh.ActionForKey(KEY_F1).has_value() == false); // freed

	HotkeyMap slots;
	CC_CHECK(slots.ActionForKey(KEY_F6).value_or("") == "SaveSlot1"); // table order
	slots.Rebind("SaveSlot1", KEY_F2);
	// F6 still fires the chord twin — rebinding plain never orphans it.
	CC_CHECK(slots.ActionForKey(KEY_F6).value_or("") == "LoadSlot1");

	HotkeyMap conflicts;
	conflicts.Rebind("AttackMove", KEY_G); // G is StanceGuard's default
	CC_CHECK(conflicts.ActionForKey(KEY_G).value_or("") == "AttackMove"); // first wins

	conflicts.ClearOverrides();
	CC_CHECK(conflicts.ActionForKey(KEY_G).value_or("") == "StanceGuard");
	CC_CHECK(conflicts.Overrides().empty());

	// Headless stand-in for Game::BindShortcuts re-running after Rebind:
	// Clear, then Bind every KeyFor.
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
