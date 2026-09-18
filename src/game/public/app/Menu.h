#pragma once

#include <string>
#include <vector>

#include "AICommander.h" // AIDifficulty for the skirmish setup
#include "MapFile.h"     // MapEntry for the setup map list
#include "Registry.h" // TeamHasUnits query
#include "Unit.h"     // health aliveness check
#include "reasings.h" // EaseQuadOut for the transition fade (pure, headless-safe)

// Menu system. MenuFlow owns the top-level game state; main.cpp
// skips the sim while paused and draws outcome/settings windows. Text and
// transitions are pure (tested); the raygui windows live in main.cpp.
// Boot-to-menu flow — the game starts at MainMenu and the world only
// builds after Start confirms (setup map + difficulty) or a slot loads.

enum class MenuState
{
    MainMenu,      // Title screen (boot state, not battle)
    SkirmishSetup, // Map + difficulty picker (v1 scope)
    Settings,      // Settings screen (shares MenuSettings with pause)
    HotkeyRemap,   // QoL remappable hotkeys (entered from Settings/pause)
    LoadGame,      // Save-slot browser (reuses the slot paths)
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
    // QoL color-blind mode (Okabe-Ito team palette in Art::TeamTint).
    // Persisted like the other settings; applied live to `art`.
    bool colorBlindMode = false;
    // Accessibility UI scale (raygui TEXT_SIZE multiplier, applied live via
    // GuiSetStyle; raw DrawText sites are explicitly out of scope).
    // Persisted like the other settings; clamped to [0.75, 2.0] on load.
    float uiScale = 1.0f;
    // QoL remapped hotkeys: (action id, raylib key) overrides only —
    // unlisted actions use kHotkeyDefs defaults. Synced from HotkeyMap
    // before every SaveSettings, applied back after every LoadSettings.
    std::vector<std::pair<std::string, int>> hotkeyOverrides;
    // Audio volumes (0..1) + mute, persisted by the settings file.
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 1.0f;
    bool mute = false;
};

// Skirmish setup. The map list is injected (main passes
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
    // Navigation (pure; main.cpp dispatches MenuAction alongside each).
    void OpenMainMenu(); // any state -> MainMenu (world teardown is the caller's job)
    void OpenSetup(const std::vector<MapEntry> &maps); // -> SkirmishSetup, selection cleared
    void OpenSettings(); // -> Settings
    void OpenLoad();     // -> LoadGame
    void SelectMap(int index); // out-of-range indices are ignored
    void SelectDifficulty(AIDifficulty difficulty);
    // SkirmishSetup -> Playing when CanStart; otherwise false (stays put).
    bool StartMatch();
};

// Standalone settings file (settings survive without a save).
inline const char *kSettingsPath = "data/settings.cfg";
// Save returns false on I/O errors. Load returns false (leaving settings
// untouched) on missing/unreadable files; malformed lines are skipped and
// every value is clamped into range.
bool SaveSettings(const MenuSettings &settings, const std::string &path);
bool LoadSettings(MenuSettings &settings, const std::string &path);

// Menu transition clock (fade-in v1): call once per frame with the live
// state; resets to 0 on change, accumulates dt otherwise. Pure, tested.
// Game renders a fullscreen fade-from-black over the first kMenuFadeInDuration
// seconds instead of snapping instantly between screens.
constexpr float kMenuFadeInDuration = 0.2f;
inline void TrackMenuTransition(MenuState &previous, float &time, MenuState current, float dt)
{
    if (current != previous)
    {
        previous = current;
        time = 0.0f;
    }
    else
    {
        time += dt;
    }
}
inline float MenuFadeAlpha(float time)
{
    // Ease-out: content snaps in fast, then settles (linear felt mechanical
    // over 0.2s). Clamped past the duration — Penner easings overshoot
    // beyond d.
    if (time >= kMenuFadeInDuration)
    {
        return 1.0f;
    }
    return EaseQuadOut(time, 0.0f, 1.0f, kMenuFadeInDuration);
}

// Any living unit (health > 0) on the given team keeps that side in the game.
bool TeamHasUnits(const Registry &registry, int teamID);
