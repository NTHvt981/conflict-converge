#pragma once

#include <functional>

#include "Audio.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "Hotkeys.h"
#include "InputManager.h"
#include "Menu.h"
#include "MenuScreens.h"
#include "Nodes.h"
#include "Pings.h"
#include "PlayingInput.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "TileMap.h"

class ShortcutBindings
{
public:
    ShortcutBindings(InputManager &input, HotkeyMap &hotkeys, MenuFlow &menu,
                     MenuScreens &menuScreens, PlayingInput &playingInput, Audio &audio,
                     GameCamera &camera, TileMap &map, OccupancyGrid &occ, ResourceNodes &nodes,
                     FogOfWar &fog, Registry &registry, ResourceSystem &resources,
                     EventDispatcher &events, const WorldState &worldState, Pings &pings,
                     const bool &worldActive, bool &showHints,
                     std::function<void()> quitToMenu, std::function<void(int)> stepReplay);
    ShortcutBindings(const ShortcutBindings &) = delete;
    ShortcutBindings &operator=(const ShortcutBindings &) = delete;

    // Clear + rebuild every Tier-1 binding (idempotent).
    void Bind();

private:
    void Announce(EventType type);

    InputManager &input_;
    HotkeyMap &hotkeys_;
    MenuFlow &menu_;
    MenuScreens &menuScreens_;
    PlayingInput &playingInput_;
    Audio &audio_;
    GameCamera &camera_;
    TileMap &map_;
    OccupancyGrid &occ_;
    ResourceNodes &nodes_;
    FogOfWar &fog_;
    Registry &registry_;
    ResourceSystem &resources_;
    EventDispatcher &events_;
    const WorldState &worldState_;
    Pings &pings_;
    const bool &worldActive_;
    bool &showHints_;
    std::function<void()> quitToMenu_;
    std::function<void(int)> stepReplay_;
};
