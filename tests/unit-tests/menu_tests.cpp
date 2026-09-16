// Unit tests for the M6 Goal 3 menu system (state transitions + aliveness).

#include "test_harness.h"

#include "Menu.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

void RunMenuTests()
{
    // --- M14: boot lands on the title screen, not in battle ---
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

    // --- M14: setup validation (no map = Start disabled) ---
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

    // --- M14: navigation between menu screens ---
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

    // --- M14: settings file roundtrip (Q86 standalone file) ---
    MenuSettings saved;
    saved.cameraSpeed = 512.5f;
    saved.showMinimap = false;
    saved.rightDragPan = true;
    saved.masterVolume = 0.5f;
    saved.musicVolume = 0.25f;
    saved.sfxVolume = 0.75f;
    saved.mute = true;
    const std::string settingsPath =
        (std::filesystem::temp_directory_path() / "cc_settings_test.cfg").string();
    CC_CHECK(SaveSettings(saved, settingsPath));
    MenuSettings loaded;
    CC_CHECK(LoadSettings(loaded, settingsPath));
    CC_CHECK(loaded.cameraSpeed == 512.5f);
    CC_CHECK(!loaded.showMinimap);
    CC_CHECK(loaded.rightDragPan);
    CC_CHECK(loaded.masterVolume == 0.5f);
    CC_CHECK(loaded.musicVolume == 0.25f);
    CC_CHECK(loaded.sfxVolume == 0.75f);
    CC_CHECK(loaded.mute);
    CC_CHECK(!LoadSettings(loaded, settingsPath + ".missing")); // untouched
    CC_CHECK(loaded.mute); // still the loaded values
    {
        std::ofstream junk(settingsPath, std::ios::trunc);
        junk << "cameraSpeed=banana\nmasterVolume=7\nshowMinimap=2\nmute=1\nunknownKey=9\n";
        junk << "rightDragPan=maybe\n";
    }
    MenuSettings strict;
    strict.cameraSpeed = 400.0f;
    strict.masterVolume = 1.0f;
    strict.showMinimap = true;
    strict.rightDragPan = true;
    strict.mute = false;
    CC_CHECK(LoadSettings(strict, settingsPath));
    CC_CHECK(strict.cameraSpeed == 400.0f); // garbage kept the old value
    CC_CHECK(strict.masterVolume == 1.0f);  // out-of-range clamped
    CC_CHECK(strict.showMinimap);           // "2" rejected
    CC_CHECK(strict.rightDragPan);          // "maybe" rejected
    CC_CHECK(strict.mute);
    std::remove(settingsPath.c_str());
}
