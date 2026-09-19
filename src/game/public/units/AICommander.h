#pragma once

#include <cstddef>
#include <vector>

#include "MathUtils.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Unit.h"
#include "UnitFactory.h"

class TileMap;        // fwd-decl (AICommander.cpp includes TileMap.h)
class OccupancyGrid;  // fwd-decl: class, not struct (TileMap.h defines it)
class ResourceNodes;  // fwd-decl (harvest target queries)
class EventDispatcher; // fwd-decl (factory event routing)

// Enemy AI commander. Plays by the same rules as the player — own
// ResourceSystem seeded with starting funds, own ProductionQueue, buildings
// placed through PlaceBuilding, orders through IssuePathOrder/IssueFormationMove.
// Bind shared occupancy via SetOccupancy and march/harvest/scout/retreat
// orders route footprint-aware (8-dir A*, occupied-goal sanitization);
// unbound (headless tests) keeps the legacy blind behavior.
// Difficulty scales build size, wave thresholds, scout cadence, and
// retreat behavior; handicaps are timer-based on Easy only, never free
// resources. Waves launch on army-size thresholds.

enum class AIDifficulty
{
    Easy,
    Medium,
    Hard
};

struct AIDifficultyParams
{
    int harvesters = 2;              // Engineers to sustain on iron
    int waveThreshold = 5;           // combat units required to launch a wave
    int reserveUnits = 0;            // extra standing army above threshold+2
    float scoutInterval = 30.0f;     // seconds between scout dispatches
    float relaunchCooldown = 20.0f;  // seconds between wave launches
    bool retreats = false;           // pull sub-30% units home (Hard only)
    std::vector<UnitType> composition; // cycled when filling the pipeline
};

AIDifficultyParams ParamsFor(AIDifficulty difficulty);

class AICommander
{
public:
    AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes, EventDispatcher &events,
                int teamID, AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile);

    // Seed funds + place Base/Depot/Factory around homeTile + spawn one
    // starting guard (so the team is never trivially wiped at frame one).
    void SetupBase();
    // Restart for a new match on a fresh world: team, difficulty, homes,
    // timers, tracked harvesters, and the owned economy/queue reset.
    // Reference members (registry/map/nodes/factory bindings) are untouched,
    // so the commander's world objects must outlive it across matches.
    // The team is resettable because nothing team-bound lives in the
    // commander: factory spawns and building placement take teamID_ per call.
    void Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile, int teamID);
    // One decision tick: income, harvesters, production, waves, scouting,
    // retreat. Safe to call every frame (cheap guards inside).
    void Update(float dt);
    // Bind shared unit occupancy for footprint-aware orders (nullable:
    // null keeps legacy blind orders). The grid must outlive the commander;
    // identity is stable across matches (Resize preserves the object), so
    // binding once per world setup suffices — Reset never clears it.
    void SetOccupancy(OccupancyGrid *occ);

    int TeamID() const;
    AIDifficulty Difficulty() const;
    int CombatUnitCount() const;   // living non-Engineer units on our team
    int HarvesterCount() const;    // living Engineers on our team
    int WavesLaunched() const;
    bool HasScouted() const;
    cc::IVec2 LastSeenEnemy() const;
    // Production requires a standing Factory (razed AI stays down).
    bool HasFactory() const;

private:
    void MaintainHarvesters();
    void MaintainProduction();
    void MaybeLaunchWave();
    void ScoutTick(float dt);
    void RetreatTick();
    void OrderHarvesterToIron(Entity harvester);
    bool FindLiveIron(cc::IVec2 &outTile) const;
    // Single-unit point order: footprint-aware when occupancy is bound,
    // legacy blind otherwise. Waves use the attack-move FP variant inline.
    void OrderMove(Unit &unit, Entity id, Vector2 dest);

    Registry &registry_;
    TileMap &map_;
    ResourceNodes &nodes_;
    OccupancyGrid *occ_ = nullptr; // bound shared grid (null = legacy orders)
    ResourceSystem resources_; // owned: fair-rules economy, seeded like the player
    UnitFactory factory_;      // bound to resources_ above
    ProductionQueue queue_;
    AIDifficultyParams params_;
    int teamID_;
    AIDifficulty difficulty_;
    cc::IVec2 homeTile_;
    cc::IVec2 enemyTile_;
    cc::IVec2 rallyTile_;
    cc::IVec2 lastSeenEnemy_;
    bool scouted_ = false;
    float scoutTimer_ = 0.0f;
    float timeSinceLaunch_ = 0.0f;
    int wavesLaunched_ = 0;
    std::size_t compIndex_ = 0;
    std::vector<Entity> harvesters_; // tracked for replacement
};
