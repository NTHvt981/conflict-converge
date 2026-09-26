#pragma once

#include <string>

#include "raylib.h"
#include "units/AICommander.h"
#include "app/Art.h"
#include "app/Audio.h"
#include "core/CheatOverlay.h"
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/GameCamera.h"
#include "core/GameRenderer.h"
#include "app/Hotkeys.h"
#include "app/Hud.h"
#include "app/InputManager.h"
#include "world/MapFile.h"
#include "core/MatchController.h"
#include "app/Menu.h"
#include "core/MenuScreens.h"
#include "app/Minimap.h"
#include "economy/Nodes.h"
#include "app/Pings.h"
#include "core/PlayingInput.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/RmlUiHost.h"
#include "app/RmlUiHud.h"
#include "app/RmlUiMenus.h"
#include "app/SaveGame.h"
#include "app/Shake.h"
#include "core/ShortcutBindings.h"
#include "core/Simulation.h"
#include "app/Skirmish.h"
#include "core/Subsystem.h"
#include "world/TileMap.h"
#include "units/UnitFactory.h"

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
