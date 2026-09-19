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
#include "TileMap.h"
#include "UnitFactory.h"

// Simulation owns the per-frame match tick extracted from Game::Update.
// It never touches input, menus (beyond the outcome gate), or rendering, so
// unit tests can drive Step headlessly over fixture worlds.
class Simulation
{
public:
    Simulation(Registry &registry, TileMap &map, OccupancyGrid &occ, FogOfWar &fog,
               ResourceNodes &nodes, ProductionQueue &queue, UnitFactory &factory,
               ResourceSystem &resources, AICommander &ai, AICommander &allyAI,
               AICommander &enemyAI2, Art &art, Audio &audio, Pings &pings, MenuFlow &menu,
               Minimap &minimap, const WorldState &worldState, DamageNumbers &damageNumbers,
               EventDispatcher &events, const Vector2 &rallyPos, const int &autoAddGroupBit,
               const bool &sandboxMode, const bool &worldIs2v2,
               const bool &playerAutoRepair, const float &autoRepairCap, float &shakeTrauma,
               MenuState &lastOutcomeState);
    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;

    // Per-match reset: edge-trigger polls, factory gate, fresh replay recording.
    void ResetForMatch();
    // Fresh edge-trigger polls + factory gate for a loaded world (load path).
    void ResetEdgePolls();
    // End recording but keep this match's frames for the viewer.
    void StopRecording();
    // Viewer entry seeds the frame count from the replay directory.
    void SetReplayCount(int count);
    int ReplayCount() const;
    // Team-0 operational Factory gate, re-runnable for the paused HUD.
    void RefreshFactory();
    bool HasFactory() const;
    // Exactly one match tick; runs only while Playing.
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
    // Match config/state owned elsewhere (read, except shakeTrauma).
    const Vector2 &rallyPos_;
    const int &autoAddGroupBit_;
    const bool &sandboxMode_;
    const bool &worldIs2v2_;
    const bool &playerAutoRepair_;
    const float &autoRepairCap_;
    float &shakeTrauma_;
    MenuState &lastOutcomeState_;
    // Edge-trigger + replay-recording state (owned here).
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
