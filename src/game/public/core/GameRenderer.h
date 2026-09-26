#pragma once

#include <functional>

#include "units/AICommander.h"
#include "app/data/Art.h"
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/ui/GameCamera.h"
#include "app/input/Hotkeys.h"
#include "app/ui/Hud.h"
#include "app/input/InputManager.h"
#include "app/match/Menu.h"
#include "core/MenuScreens.h"
#include "app/ui/Minimap.h"
#include "economy/Nodes.h"
#include "app/ui/Pings.h"
#include "core/PlayingInput.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/ui/RmlUiHud.h"
#include "core/Simulation.h"
#include "app/ui/Shake.h"
#include "world/TileMap.h"

class RmlUiHost;
class RmlUiMenus;

class GameRenderer
{
public:
    GameRenderer(Art &art, GameCamera &camera, TileMap &map, Registry &registry,
                 FogOfWar &fog, ResourceNodes &nodes, Minimap &minimap, Pings &pings,
                 DamageNumbers &damageNumbers, PlayingInput &playingInput, MenuFlow &menu,
                 Simulation &sim, ResourceSystem &resources, ProductionQueue &queue,
                 HotkeyMap &hotkeys, InputManager &input, AICommander &ai,
                 EventDispatcher &events, const int &replayCursor,
                 const AIDifficulty &worldDifficulty, const float &menuStateTime,
                 const float &shakeTrauma, RmlUiHost &rmlUi, RmlUiHud &rmlUiHud,
                 RmlUiMenus &rmlUiMenus, std::function<void()> quitToMenu);    GameRenderer(const GameRenderer &) = delete;
    GameRenderer &operator=(const GameRenderer &) = delete;

    void DrawWorld();
    ConfirmChoice DrawHudAndOverlays(int screenWidth, int screenHeight, float uiScale);

private:
    Art &art_;
    GameCamera &camera_;
    TileMap &map_;
    Registry &registry_;
    FogOfWar &fog_;
    ResourceNodes &nodes_;
    Minimap &minimap_;
    Pings &pings_;
    DamageNumbers &damageNumbers_;
    PlayingInput &playingInput_;
    MenuFlow &menu_;
    Simulation &sim_;
    ResourceSystem &resources_;
    ProductionQueue &queue_;
    HotkeyMap &hotkeys_;
    InputManager &input_;
    AICommander &ai_;
    EventDispatcher &events_;
    const int &replayCursor_;
    const AIDifficulty &worldDifficulty_;
    const float &menuStateTime_;
    const float &shakeTrauma_;
    RmlUiHost &rmlUi_;
    RmlUiHud &rmlUiHud_;
    RmlUiMenus &rmlUiMenus_;
    std::function<void()> quitToMenu_;
    HoverTooltipState hoverTip_;
};
