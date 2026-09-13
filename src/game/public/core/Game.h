#pragma once

#include <string>

#include "raylib.h"
#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "InputManager.h"
#include "MapFile.h"
#include "Menu.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "Skirmish.h"
#include "TileMap.h"
#include "UnitFactory.h"

// Game owns the former main.cpp boot state + frame loop. main.cpp is only
// the entry point (construct, Run, return). Members are declared in
// construction-dependency order: factory/ai/skirmish/worldState hold
// references/pointers into the registries above them, so reordering breaks
// them. Non-copyable/non-movable for the same reason (shortcut lambdas
// capture this).
class Game
{
public:
    Game();
    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    // Lifecycle (previously the sections of main()).
    void Init();     // window, audio/art, state, shortcut bindings
    void Update();   // exactly one frame (input pump + sim + render)
    void Shutdown(); // unload + CloseWindow
    bool IsRunning() const;
    // Convenience for main(): Init + loop + Shutdown.
    int Run();

private:
    void Announce(EventType type);
    void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
    void QuitToMenu();
    void BindShortcuts();

    // Former boot-level locals in main(), same order.
    Audio audio;
    Art art;
    GameCamera camera;
    MenuFlow menu;
    Minimap minimap;
    Registry registry;
    ResourceSystem resources;
    EventDispatcher events;
    TileMap map;
    FogOfWar fog;
    ResourceNodes nodes;
    ProductionQueue queue;
    UnitFactory factory;
    AICommander ai;
    Vector2 rallyPos = {};
    SkirmishWorld skirmish;
    WorldState worldState;
    InputManager input;

    bool worldActive = false;
    AIDifficulty worldDifficulty = AIDifficulty::Medium;
    std::string worldMapPath;

    bool settingRally = false;
    bool dragging = false;
    Vector2 dragStart = {};
    bool showHints = true;
    int setupScroll = 0;

    // Per-frame poll state (M11 edge-triggered sounds, M14 transitions).
    int lastBuildingCount = 0;
    int lastDepletedCount = 0;
    int lastQueueSize = 0;
    float attackSfxTimer = 0.0f;
    MenuState lastOutcomeState = MenuState::MainMenu;
    bool hasFactory = false;
};
