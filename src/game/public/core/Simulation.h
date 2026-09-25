#pragma once

#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "FogOfWar.h"
#include "Menu.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Pings.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "Subsystem.h"
#include "TileMap.h"
#include "UnitFactory.h"

// It never touches input, menus (beyond the outcome gate), or rendering, so
// unit tests can drive Step headlessly over fixture worlds.
class Simulation
{
public:
    Simulation(Subsystems &world, Subsystems &engine, MenuFlow &menu,
               const WorldState &worldState, DamageNumbers &damageNumbers,
               const Vector2 &rallyPos, const int &autoAddGroupBit,
               const bool &sandboxMode, const bool &worldIs2v2,
               const bool &playerAutoRepair, const float &autoRepairCap,
               float &shakeTrauma, MenuState &lastOutcomeState);
    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;

    void ResetForMatch();
    void ResetEdgePolls();
    void StopRecording();
    void SetReplayCount(int count);
    int ReplayCount() const;
    // Re-runnable for the paused HUD.
    void RefreshFactory();
    bool HasFactory() const;
    // Runs only while Playing.
    void Step(float dt);

private:
    void Announce(EventType type);

    Registry &registry_;
    TileMap &map_;
    OccupancyGrid &occ_;
    FogOfWar &fog_;
    ResourceNodes &nodes_;
    ProductionQueue &queue_;
    UnitFactory &factory_;
    ResourceSystem &resources_;
    AICommander &ai_;
    AICommander &allyAI_;
    AICommander &enemyAI2_;
    Art &art_;
    Audio &audio_;
    Pings &pings_;
    MenuFlow &menu_;
    Minimap &minimap_;
    const WorldState &worldState_;
    DamageNumbers &damageNumbers_;
    EventDispatcher &events_;
    const Vector2 &rallyPos_;
    const int &autoAddGroupBit_;
    const bool &sandboxMode_;
    const bool &worldIs2v2_;
    const bool &playerAutoRepair_;
    const float &autoRepairCap_;
    float &shakeTrauma_;
    MenuState &lastOutcomeState_;
    int lastBuildingCount_ = 0;
    int lastDepletedCount_ = 0;
    int lastQueueSize_ = 0;
    float attackSfxTimer_ = 0.0f;
    bool hasFactory_ = false;
    bool replayRecording_ = false;
    float replayTimer_ = 0.0f;
    int replayIndex_ = 0;
    int replayCount_ = 0;
};
