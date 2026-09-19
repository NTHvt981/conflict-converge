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

// Simulation owns the per-frame match tick extracted from Game::Update
// (visibility, movement, death sweep, edge-triggered audio, economy,
// production, replay capture, auto-repair/retreat, AI, minimap refresh,
// outcome) plus the edge-trigger and replay-recording state that only the
// tick reads and writes. Everything else is a reference into the world
// the simulation steps — same precedent as AICommander/UnitFactory, which
// hold reference bundles into their owners' registries. The class never
// touches input, menus (beyond the outcome gate), or rendering, so unit
// tests can drive Step headlessly over fixture worlds.
// Non-copyable: reference members bind the owner's storage for life.
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

    // Per-match reset: edge-trigger polls, factory gate, and a fresh
    // replay recording (replaces the StartMatch reset block).
    void ResetForMatch();
    // Fresh edge-trigger polls + factory gate for a loaded world, without
    // touching the replay recording (load path only).
    void ResetEdgePolls();
    // End recording but keep this match's frames for the viewer
    // (QuitToMenu / viewer entry).
    void StopRecording();
    // Viewer entry seeds the frame count from the replay directory.
    void SetReplayCount(int count);
    int ReplayCount() const;
    // Team-0 operational Factory gate: recomputed by Step, and re-runnable
    // for the HUD so the production panel stays correct while paused.
    void RefreshFactory();
    bool HasFactory() const;
    // Exactly one match tick. Runs only while Playing (the gate stays
    // with the caller).
    void Step(float dt);

private:
    // Bare-event announcer for the game-state + UI event types.
    void Announce(EventType type);

    // World under test (owned elsewhere, bound for life).
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
    // Outcome-transition edge (menu flow owns it: match setup seeds
    // Playing, the tick's fanfare block advances it).
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
