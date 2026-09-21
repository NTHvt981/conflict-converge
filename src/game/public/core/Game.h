#pragma once

#include <string>

#include "raylib.h"
#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "CheatOverlay.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "GameRenderer.h"
#include "Hotkeys.h"
#include "Hud.h"
#include "InputManager.h"
#include "MapFile.h"
#include "MatchController.h"
#include "Menu.h"
#include "MenuScreens.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Pings.h"
#include "PlayingInput.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "RmlUiHost.h"
#include "RmlUiHud.h"
#include "RmlUiMenus.h"
#include "SaveGame.h"
#include "Shake.h"
#include "ShortcutBindings.h"
#include "Simulation.h"
#include "Skirmish.h"
#include "TileMap.h"
#include "UnitFactory.h"

// Game owns the boot state + frame loop; main.cpp is entry-only
// (construct, Run, return). Non-copyable: shortcut lambdas capture this.
class Game
{
public:
    Game();
    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    void Init();     // window, audio/art, state, shortcut bindings
    void Update();   // exactly one frame (input pump + sim + render)
    void Shutdown(); // unload + CloseWindow
    bool IsRunning() const;
    // Convenience for main: Init + loop + Shutdown.
    int Run();

protected:
    // Test-seam access for E2EGame (tests/e2e): the match lifecycle the
    // headless harness drives directly. Everything else stays private.
    void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
    void QuitToMenu();
private:
    Audio audio;
    Art art;
    GameCamera camera;
protected:
    MenuFlow menu;    // E2EGame seam
private:
    Minimap minimap;
    Pings pings; // attack/event pings (minimap blips + camera jump)
protected:
    Registry registry; // E2EGame seam
private:
    ResourceSystem resources;
    EventDispatcher events;
protected:
    TileMap map; // E2EGame seam
private:
    OccupancyGrid occ; // unit/building tile occupancy
    FogOfWar fog;
    ResourceNodes nodes;
    ProductionQueue queue;
    UnitFactory factory;
    AICommander ai;
    // 2v2 overflow commanders (declared after ai); armed only on 2v2 maps.
    AICommander allyAI;
    AICommander enemyAI2;
    Vector2 rallyPos = {};
    SkirmishWorld skirmish;
    WorldState worldState;
    InputManager input;
    HotkeyMap hotkeys; // remappable Tier-1 keys

protected:
    bool worldActive = false; // E2EGame seam
private:
    // Gates the allyAI/enemyAI2 ticks; 2v2 only (see Game.h.context.md).
    bool worldIs2v2 = false;
    bool sandboxMode = false;
    AIDifficulty worldDifficulty = AIDifficulty::Medium;
    std::string worldMapPath;

    int replayCursor = 0; // currently shown frame
    float replayPlayTimer = 0.0f; // auto-advance clock in the viewer
    bool showHints = true;
    bool playerAutoRepair = false;
    float autoRepairCap = 1.0f;
    float shakeTrauma = 0.0f; // screen-shake trauma 0..1 (render copy only)
    DamageNumbers damageNumbers; // floating hit numbers (presentation-only)
    MenuState previousMenuState = MenuState::MainMenu; // transition-fade clock
    float menuStateTime = 0.0f;
    MenuState lastOutcomeState = MenuState::MainMenu;
    ConfirmChoice pendingConfirm = ConfirmChoice::None; // resolved next frame
    // Input dispatch + match tick (declared after the members they bind).
    PlayingInput playingInput;
    Simulation sim;
    MenuScreens menuScreens;
    ShortcutBindings bindings;
    RmlUiHost rmlUi; // declared before renderer: overlay host (Phase 1 render-only)
    RmlUiMenus rmlUiMenus; // menu-branch owner (Phase 2 states, raygui keeps the rest)
    RmlUiHud rmlUiHud; // in-match HUD panels (Phase 3, raygui fallback when not ready)
    GameRenderer renderer;
    MatchController match;
    CheatOverlay cheats;
};
