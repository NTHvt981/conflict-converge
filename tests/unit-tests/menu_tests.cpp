// Unit tests for the menu system (state transitions + aliveness).

#include "test_harness.h"

#include "app/match/Menu.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

void RunMenuTests()
{
    // --- Boot lands on the title screen, not in battle ---
    MenuFlow menu;
    CC_CHECK(menu.state == MenuState::MainMenu);
    // --- pause toggle only touches Playing/Paused ---
    menu.state = MenuState::Playing;
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::Paused);
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::Playing);

    menu.state = MenuState::GameOver;
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::GameOver);
    menu.state = MenuState::Victory;
    menu.TogglePause();
    CC_CHECK(menu.state == MenuState::Victory);

    // --- outcome matrix; player loss wins ties ---
    menu.state = MenuState::Playing;
    menu.ShowOutcome(true, true);
    CC_CHECK(menu.state == MenuState::Playing);
    menu.ShowOutcome(true, false);
    CC_CHECK(menu.state == MenuState::Victory);
    menu.state = MenuState::Playing;
    menu.ShowOutcome(false, true);
    CC_CHECK(menu.state == MenuState::GameOver);
    menu.state = MenuState::Playing;
    menu.ShowOutcome(false, false);
    CC_CHECK(menu.state == MenuState::GameOver);

    // --- defaults ---
    MenuFlow fresh;
    CC_CHECK(fresh.settings.cameraSpeed == 400.0f);
    CC_CHECK(fresh.settings.showMinimap);
    CC_CHECK(!fresh.settings.showHints); // hints hide by default
    CC_CHECK(!fresh.quitRequested);

    // --- team aliveness ignores corpses and other teams ---
    Registry registry;
    CC_CHECK(!TeamHasUnits(registry, 0));

    Unit ally;
    ally.teamID = 0;
    ally.health = 10.0f;
    const Entity allyId = registry.Create();
    registry.Add(allyId, ally);
    CC_CHECK(TeamHasUnits(registry, 0));
    CC_CHECK(!TeamHasUnits(registry, 1));

    Unit *stored = registry.Get<Unit>(allyId);
    stored->health = 0.0f; // corpse: side is out
    CC_CHECK(!TeamHasUnits(registry, 0));

    Unit foe;
    foe.teamID = 1;
    foe.health = 50.0f;
    registry.Add(registry.Create(), foe);
    CC_CHECK(TeamHasUnits(registry, 1));
    CC_CHECK(!TeamHasUnits(registry, 0));

    // --- Setup validation (no map = Start disabled) ---
    MenuFlow boot;
    CC_CHECK(boot.state == MenuState::MainMenu);
    CC_CHECK(!boot.setup.CanStart());
    CC_CHECK(boot.setup.SelectedMap() == nullptr);
    CC_CHECK(!boot.StartMatch()); // stays put without a map
    CC_CHECK(boot.state == MenuState::MainMenu);

    MapEntry cross;
    cross.path = "data/maps/crossroads.map";
    cross.name = "Crossroads";
    cross.width = 24;
    cross.height = 18;
    MapEntry basins;
    basins.path = "data/maps/twin_basins.map";
    basins.name = "Twin Basins";
    basins.width = 24;
    basins.height = 18;
    boot.OpenSetup({ cross, basins });
    CC_CHECK(boot.state == MenuState::SkirmishSetup);
    CC_CHECK(!boot.setup.CanStart());
    CC_CHECK(boot.setup.difficulty == AIDifficulty::Medium); // default
    boot.SelectMap(7); // out of range: ignored
    CC_CHECK(!boot.setup.CanStart());
    boot.SelectMap(-1);
    CC_CHECK(!boot.setup.CanStart());
    boot.SelectMap(1);
    CC_CHECK(boot.setup.CanStart());
    CC_CHECK(boot.setup.SelectedMap()->name == "Twin Basins");
    boot.OpenSetup({ cross, basins }); // reopening clears the selection
    CC_CHECK(!boot.setup.CanStart());
    boot.SelectMap(0);
    boot.SelectDifficulty(AIDifficulty::Hard);
    CC_CHECK(boot.setup.difficulty == AIDifficulty::Hard);
    CC_CHECK(boot.StartMatch());
    CC_CHECK(boot.state == MenuState::Playing);
    CC_CHECK(!boot.StartMatch()); // only from the setup screen

    // --- Navigation between menu screens ---
    boot.OpenMainMenu();
    CC_CHECK(boot.state == MenuState::MainMenu);
    boot.OpenSettings();
    CC_CHECK(boot.state == MenuState::Settings);
    boot.OpenLoad();
    CC_CHECK(boot.state == MenuState::LoadGame);
    boot.TogglePause(); // menu screens ignore the pause toggle
    CC_CHECK(boot.state == MenuState::LoadGame);
    boot.ShowOutcome(false, true); // terminal states still reachable
    CC_CHECK(boot.state == MenuState::GameOver);

    // --- Esc/Back policy: modals, submenu back, direct teardown ---
    {
        MenuFlow esc;
        CC_CHECK(!esc.ConfirmOpen());
        // MainMenu: ask before quitting to desktop
        CC_CHECK(esc.OnBackPressed() == BackAction::OpenQuitConfirm);
        CC_CHECK(esc.confirm == ConfirmKind::QuitApp);
        CC_CHECK(esc.OnBackPressed() == BackAction::None); // Esc cancels the modal
        CC_CHECK(!esc.ConfirmOpen());
        // Sub-menus navigate back to the title
        esc.OpenSetup({});
        CC_CHECK(esc.OnBackPressed() == BackAction::Navigate);
        CC_CHECK(esc.state == MenuState::MainMenu);
        esc.OpenSettings();
        CC_CHECK(esc.OnBackPressed() == BackAction::Navigate);
        CC_CHECK(esc.state == MenuState::MainMenu);
        esc.OpenLoad();
        CC_CHECK(esc.OnBackPressed() == BackAction::Navigate);
        CC_CHECK(esc.state == MenuState::MainMenu);
        // Playing/Paused: confirm abandoning the match
        esc.state = MenuState::Playing;
        CC_CHECK(esc.OnBackPressed() == BackAction::OpenBackToMenuConfirm);
        CC_CHECK(esc.confirm == ConfirmKind::BackToMenu);
        esc.CloseConfirm();
        esc.state = MenuState::Paused;
        CC_CHECK(esc.OnBackPressed() == BackAction::OpenBackToMenuConfirm);
        CC_CHECK(esc.confirm == ConfirmKind::BackToMenu);
        esc.CloseConfirm();
        // Terminal/outcome/replay: straight back to the menu (no modal)
        esc.state = MenuState::GameOver;
        CC_CHECK(esc.OnBackPressed() == BackAction::QuitToMenu);
        CC_CHECK(!esc.ConfirmOpen());
        esc.state = MenuState::Victory;
        CC_CHECK(esc.OnBackPressed() == BackAction::QuitToMenu);
        esc.state = MenuState::ReplayViewer;
        CC_CHECK(esc.OnBackPressed() == BackAction::QuitToMenu);
        // HotkeyRemap is owned by MenuScreens::CancelRemapCapture
        esc.state = MenuState::HotkeyRemap;
        CC_CHECK(esc.OnBackPressed() == BackAction::None);
    }

    // --- Settings file roundtrip (standalone file) ---
    MenuSettings saved;
    saved.cameraSpeed = 512.5f;
    saved.showMinimap = false;
    saved.showHints = true;
    saved.rightDragPan = true;
    saved.colorBlindMode = true;
    saved.uiScale = 1.5f;
    saved.masterVolume = 0.5f;
    saved.musicVolume = 0.25f;
    saved.sfxVolume = 0.75f;
    saved.mute = true;
    saved.hotkeyOverrides = { { "ToggleHints", KEY_F2 }, { "Halt", KEY_H } };
    const std::string settingsPath =
        (std::filesystem::temp_directory_path() / "cc_settings_test.cfg").string();
    CC_CHECK(SaveSettings(saved, settingsPath));
    MenuSettings loaded;
    CC_CHECK(LoadSettings(loaded, settingsPath));
    CC_CHECK(loaded.cameraSpeed == 512.5f);
    CC_CHECK(!loaded.showMinimap);
    CC_CHECK(loaded.showHints);
    CC_CHECK(loaded.rightDragPan);
    CC_CHECK(loaded.colorBlindMode);
    CC_CHECK(loaded.uiScale == 1.5f);
    CC_CHECK(loaded.masterVolume == 0.5f);
    CC_CHECK(loaded.musicVolume == 0.25f);
    CC_CHECK(loaded.sfxVolume == 0.75f);
    CC_CHECK(loaded.mute);
    CC_CHECK(loaded.hotkeyOverrides.size() == 2);
    CC_CHECK(loaded.hotkeyOverrides[0].first == "ToggleHints");
    CC_CHECK(loaded.hotkeyOverrides[0].second == KEY_F2);
    CC_CHECK(loaded.hotkeyOverrides[1].first == "Halt");
    CC_CHECK(loaded.hotkeyOverrides[1].second == KEY_H);
    CC_CHECK(!LoadSettings(loaded, settingsPath + ".missing")); // untouched
    CC_CHECK(loaded.mute); // still the loaded values
    {
        std::ofstream junk(settingsPath, std::ios::trunc);
        junk << "cameraSpeed=banana\nmasterVolume=7\nshowMinimap=2\nmute=1\nunknownKey=9\n";
        junk << "rightDragPan=maybe\n";
        junk << "colorBlindMode=maybe\n";
        junk << "uiScale=banana\n";
        junk << "hotkey.ToggleHints=" << KEY_F2 << "\n"; // valid override
        junk << "hotkey.Spaceship=294\n";     // unknown action: skipped
        junk << "hotkey.Halt=banana\n";       // malformed key: skipped
        junk << "hotkey.Halt=0\n";            // non-positive key: skipped
        junk << "hotkey.Halt=" << KEY_H << "\n"; // valid, later line wins
    }
    MenuSettings strict;
    strict.cameraSpeed = 400.0f;
    strict.masterVolume = 1.0f;
    strict.showMinimap = true;
    strict.rightDragPan = true;
    strict.colorBlindMode = true;
    strict.uiScale = 1.0f;
    strict.hotkeyOverrides = { { "Halt", KEY_SPACE } }; // replaced below
    strict.mute = false;
    CC_CHECK(LoadSettings(strict, settingsPath));
    CC_CHECK(strict.cameraSpeed == 400.0f); // garbage kept the old value
    CC_CHECK(strict.masterVolume == 1.0f);  // out-of-range clamped
    CC_CHECK(strict.showMinimap);           // "2" rejected
    CC_CHECK(strict.rightDragPan);          // "maybe" rejected
    CC_CHECK(strict.colorBlindMode);        // "maybe" rejected
    CC_CHECK(strict.uiScale == 1.0f);       // "banana" rejected
    CC_CHECK(strict.hotkeyOverrides.size() == 2); // Spaceship/banana/0 skipped
    CC_CHECK(strict.hotkeyOverrides[0].first == "Halt");
    CC_CHECK(strict.hotkeyOverrides[0].second == KEY_H); // replaced, not appended
    CC_CHECK(strict.hotkeyOverrides[1].first == "ToggleHints");
    CC_CHECK(strict.hotkeyOverrides[1].second == KEY_F2); // new entry appended
    CC_CHECK(strict.mute);
    {
        std::ofstream clamp(settingsPath, std::ios::trunc);
        clamp << "uiScale=9\n";
    }
    MenuSettings hi;
    CC_CHECK(LoadSettings(hi, settingsPath));
    CC_CHECK(hi.uiScale == 2.0f); // out-of-range clamps, not garbage
    {
        std::ofstream clamp(settingsPath, std::ios::trunc);
        clamp << "uiScale=0.1\n";
    }
    MenuSettings lo;
    CC_CHECK(LoadSettings(lo, settingsPath));
    CC_CHECK(lo.uiScale == 0.75f);
    std::remove(settingsPath.c_str());

    // --- menu transition clock: resets on change, accumulates otherwise ---
    MenuState previous = MenuState::MainMenu;
    float time = 0.0f;
    TrackMenuTransition(previous, time, MenuState::MainMenu, 0.1f);
    CC_CHECK(time == 0.1f); // same state: accumulates
    CC_CHECK(MenuFadeAlpha(time) < 1.0f); // mid-fade
    TrackMenuTransition(previous, time, MenuState::Settings, 0.1f);
    CC_CHECK(previous == MenuState::Settings);
    CC_CHECK(time == 0.0f); // changed state: resets, dt not added
    CC_CHECK(MenuFadeAlpha(time) == 0.0f);
    TrackMenuTransition(previous, time, MenuState::Settings, kMenuFadeInDuration);
    CC_CHECK(MenuFadeAlpha(time) == 1.0f); // full duration: opaque-free
    TrackMenuTransition(previous, time, MenuState::Settings, 10.0f);
    CC_CHECK(MenuFadeAlpha(time) == 1.0f); // clamps, never exceeds 1
    // Ease-out, not linear: halfway through time the fade is 3/4 done.
    CC_CHECK(MenuFadeAlpha(kMenuFadeInDuration / 2.0f) == 0.75f);
}
