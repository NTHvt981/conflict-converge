#pragma once

#include <string>
#include <vector>

#include "AICommander.h" // AIDifficulty for the skirmish setup
#include "MapFile.h"     // MapEntry for the setup map list
#include "Registry.h" // TeamHasUnits query
#include "Unit.h"     // health aliveness check

// M6 Goal 3: menu system. MenuFlow owns the top-level game state; main.cpp
// skips the sim while paused and draws outcome/settings windows. Text and
// transitions are pure (tested); the raygui windows live in main.cpp.
// M14: boot-to-menu flow — the game starts at MainMenu and the world only
// builds after Start confirms (setup map + difficulty) or a slot loads.

enum class MenuState
{
    MainMenu,      // M14: title screen (boot state, not battle)
    SkirmishSetup, // M14: map + difficulty picker (Q85: v1 scope)
    Settings,      // M14: settings screen (shares MenuSettings with pause)
    LoadGame,      // M14: save-slot browser (reuses the M13 slot paths)
    Playing,
    Paused,
    GameOver,
    Victory,
    ReplayViewer, // QoL snapshot replay: world renders from a loaded
                  // snapshot, sim stays frozen (Playing-gated systems skip
                  // this state exactly like they skip menus)
    MapEditor // QoL map editor: scratch-canvas painter (never the live
              // match map); runs inside the no-world menu branch
};

struct MenuSettings
{
    float cameraSpeed = 400.0f;
    bool showMinimap = true;
    // QoL right-drag camera pan (opt-in; default keeps right-click purely
    // for orders). Persisted below like the other settings.
    bool rightDragPan = false;
    // M11: audio volumes (0..1) + mute, persisted by the M14 settings file.
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 1.0f;
    bool mute = false;
};

// M14: skirmish setup. The map list is injected (main passes
// ListMaps("data/"), tests pass fakes); nothing here touches the
// filesystem or raygui, so the whole flow stays headless-testable.
struct SkirmishSetup
{
    std::vector<MapEntry> maps;
    int mapIndex = -1; // nothing selected: Start is disabled
    AIDifficulty difficulty = AIDifficulty::Medium;

    bool CanStart() const
    {
        return mapIndex >= 0 && mapIndex < static_cast<int>(maps.size());
    }
    const MapEntry *SelectedMap() const
    {
        return CanStart() ? &maps[static_cast<std::size_t>(mapIndex)] : nullptr;
    }
};

struct MenuFlow
{
    MenuState state = MenuState::MainMenu;
    MenuSettings settings;
    SkirmishSetup setup;
    bool quitRequested = false;

    // Playing <-> Paused only; outcome screens are terminal until quit.
    void TogglePause();
    // Decide terminal states from team aliveness (call each frame while Playing).
    // Player loss takes priority when both sides are wiped.
    void ShowOutcome(bool playerAlive, bool enemyAlive);
    // M14 navigation (pure; main.cpp dispatches MenuAction alongside each).
    void OpenMainMenu(); // any state -> MainMenu (world teardown is the caller's job)
    void OpenSetup(const std::vector<MapEntry> &maps); // -> SkirmishSetup, selection cleared
    void OpenSettings(); // -> Settings
    void OpenLoad();     // -> LoadGame
    void SelectMap(int index); // out-of-range indices are ignored
    void SelectDifficulty(AIDifficulty difficulty);
    // SkirmishSetup -> Playing when CanStart; otherwise false (stays put).
    bool StartMatch();
};

// M14: standalone settings file (Q86: settings survive without a save).
inline const char *kSettingsPath = "data/settings.cfg";
// Save returns false on I/O errors. Load returns false (leaving settings
// untouched) on missing/unreadable files; malformed lines are skipped and
// every value is clamped into range.
bool SaveSettings(const MenuSettings &settings, const std::string &path);
bool LoadSettings(MenuSettings &settings, const std::string &path);

// Any living unit (health > 0) on the given team keeps that side in the game.
bool TeamHasUnits(const Registry &registry, int teamID);
