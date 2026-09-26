#pragma once

#include <functional>

#include "app/Audio.h"
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/GameCamera.h"
#include "app/Hotkeys.h"
#include "app/InputManager.h"
#include "app/Menu.h"
#include "economy/Nodes.h"
#include "app/Pings.h"
#include "core/PlayingInput.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/RmlUiMenus.h"
#include "app/SaveGame.h"
#include "world/TileMap.h"

class ShortcutBindings
{
public:
    ShortcutBindings(InputManager &input, HotkeyMap &hotkeys, MenuFlow &menu,
                     RmlUiMenus &rmlUiMenus, PlayingInput &playingInput,
                     Audio &audio, GameCamera &camera, TileMap &map, OccupancyGrid &occ,
                     ResourceNodes &nodes, FogOfWar &fog, Registry &registry,
                     ResourceSystem &resources, EventDispatcher &events,
                     const WorldState &worldState, Pings &pings, const bool &worldActive,
                     bool &showHints, std::function<void()> quitToMenu,
                     std::function<void(int)> stepReplay);
    ShortcutBindings(const ShortcutBindings &) = delete;
    ShortcutBindings &operator=(const ShortcutBindings &) = delete;

    // Idempotent.
    void Bind();

private:
    void Announce(EventType type);

    InputManager &input_;
    HotkeyMap &hotkeys_;
    MenuFlow &menu_;
    RmlUiMenus &rmlUiMenus_;
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
