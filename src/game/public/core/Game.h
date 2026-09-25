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
#include "Subsystem.h"
#include "TileMap.h"
#include "UnitFactory.h"

// Non-copyable: shortcut lambdas capture this.
class Game
{
public:
    Game();
    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    void Init();
    void Update();
    void Shutdown();
    bool IsRunning() const;
    int Run();

protected:
    // Test-seam access for E2EGame (tests/e2e): the match lifecycle the
    // headless harness drives directly. Everything else stays private.
    void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
    void QuitToMenu();
private:
    // Engine-lifetime service scope: owns Audio, Art, EventDispatcher,
    // InputManager, and HotkeyMap. First member, constructed before every
    // consumer, so init-list expressions may bind engine_.Get<T>() references.
    // The Adds run in the EngineScope ctor; Game::Init body would be too late.
    struct EngineScope : public Subsystems
    {
        EngineScope();
    };
    EngineScope engine_;
protected:
    // World/match-lifetime service scope: owns Registry, ResourceSystem,
    // TileMap, OccupancyGrid, FogOfWar, ResourceNodes, ProductionQueue,
    // UnitFactory, three keyed AICommanders, Pings, and Minimap. Declared
    // after engine_ (factory/AI bind the engine EventDispatcher) and before
    // every consumer, so init-list expressions may bind world_.Get<T>().
    // Protected: E2EGame reaches the registry/map through it.
    struct WorldScope : public Subsystems
    {
        explicit WorldScope(Subsystems &engine);
    };
    WorldScope world_{engine_}; // E2EGame seam (registry/map access)
    MenuFlow menu;    // E2EGame seam (moved before player_: PlayerScope binds menu.settings)
private:
    Vector2 rallyPos = {}; // moved before player_: PlayerScope forwards it to PlayingInput
    // Player/view-state scope: owns GameCamera + PlayingInput. Declared after
    // engine_/world_/menu/rallyPos (PlayingInput binds them all) and before
    // every consumer, so init-list expressions may bind player_.Get<T>().
    struct PlayerScope : public Subsystems
    {
        PlayerScope(Subsystems &engine, Subsystems &world, const MenuSettings &settings,
                    Vector2 &rallyPos);
    };
    PlayerScope player_{engine_, world_, menu.settings, rallyPos};
    SkirmishWorld skirmish;
    WorldState worldState;
protected:
    bool worldActive = false; // E2EGame seam
private:
    bool worldIs2v2 = false;
    bool sandboxMode = false;
    AIDifficulty worldDifficulty = AIDifficulty::Medium;
    std::string worldMapPath;

    int replayCursor = 0;
    float replayPlayTimer = 0.0f;
    bool showHints = true;
    float shakeTrauma = 0.0f; // 0..1, render copy only
    DamageNumbers damageNumbers;
    MenuState previousMenuState = MenuState::MainMenu;
    float menuStateTime = 0.0f;
    MenuState lastOutcomeState = MenuState::MainMenu;
    ConfirmChoice pendingConfirm = ConfirmChoice::None; // resolved next frame
    // Match tick (declared after the members it binds).
    Simulation sim;
    MenuScreens menuScreens;
    // Declared before bindings: the Back shortcut prefers the RML remap
    // capture while it owns the HotkeyRemap state.
    RmlUiMenus rmlUiMenus;
    ShortcutBindings bindings;
    RmlUiHost rmlUi; // declared before renderer: overlay host (Phase 1 render-only)
    RmlUiHud rmlUiHud; // in-match HUD panels (Phase 3, raygui fallback when not ready)
    GameRenderer renderer;
    MatchController match;
    CheatOverlay cheats;
};
