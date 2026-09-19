#pragma once

#include <string>

#include "raylib.h"
#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "Hotkeys.h"
#include "Hud.h"
#include "InputManager.h"
#include "MapFile.h"
#include "Menu.h"
#include "MenuScreens.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Pings.h"
#include "PlayingInput.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "Shake.h"
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

    // E2E driver seam (tests/e2e only): drives the real loop headlessly.
    MenuFlow &E2EMenu();
    Registry &E2ERegistry();
    TileMap &E2EMap();
    bool IsWorldActive() const;
    // SelectedMap + difficulty -> StartMatch; false when nothing selected.
    bool E2EStartSelectedMatch();
    void E2EQuitToMenu();

private:
    void Announce(EventType type);
    void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
    void QuitToMenu();
    void LoadGameFromSlot(const std::string &slotPath);
    void BindShortcuts();
    void StepReplay(int dir);
    bool WatchLastReplay();
    void DrawWorld();
    void DrawHudAndOverlays(int screenWidth, int screenHeight);
    // Esc-modal keys (Y = yes, N/Back = no); deferred so world teardown never
    // runs mid-frame.
    void PollConfirmKeys();
    void ApplyConfirmChoice(ConfirmChoice choice);

    Audio audio;
    Art art;
    GameCamera camera;
    MenuFlow menu;
    Minimap minimap;
    Pings pings; // attack/event pings (minimap blips + camera jump)
    Registry registry;
    ResourceSystem resources;
    EventDispatcher events;
    TileMap map;
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

    bool worldActive = false;
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
    HoverTooltipState hoverTip; // unit hover-tooltip debounce (Playing only)
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
};
