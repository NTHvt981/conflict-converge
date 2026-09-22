#pragma once

#include <cstddef>
#include <vector>

#include "MathUtils.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Subsystem.h"
#include "Unit.h"
#include "UnitFactory.h"

class TileMap;
class OccupancyGrid;
class ResourceNodes;
class EventDispatcher;

// Enemy AI commander: own fair-rules economy, production, building placement,
// and orders. Difficulty scales build size, wave thresholds, scout cadence,
// and retreat behavior.

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

class AICommander : public Subsystem
{
public:
    AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes, EventDispatcher &events,
                int teamID, AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile);

    // Seed funds + place Base/Depot/Factory around homeTile + one starting guard.
    void SetupBase();
    // Restart for a new match: team, difficulty, homes, timers, harvesters,
    // and the owned economy/queue. Reference members are untouched.
    void Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile, int teamID);
    // One decision tick: income, harvesters, production, waves, scouting, retreat.
    void Update(float dt);
    // Bind shared unit occupancy for footprint-aware orders (null = legacy).
    void SetOccupancy(OccupancyGrid *occ);

    int TeamID() const;
    AIDifficulty Difficulty() const;
    int CombatUnitCount() const;   // living non-Engineer units on our team
    int HarvesterCount() const;    // living Engineers on our team
    int WavesLaunched() const;
    bool HasScouted() const;
    cc::IVec2 LastSeenEnemy() const;
    bool HasFactory() const;

private:
    void MaintainHarvesters();
    void MaintainProduction();
    void MaybeLaunchWave();
    void ScoutTick(float dt);
    void RetreatTick();
    void OrderHarvesterToIron(Entity harvester);
    bool FindLiveIron(cc::IVec2 &outTile) const;
    // Single-unit point order: footprint-aware when occupancy is bound.
    void OrderMove(Unit &unit, Entity id, Vector2 dest);

    Registry &registry_;
    TileMap &map_;
    ResourceNodes &nodes_;
    OccupancyGrid *occ_ = nullptr;
    ResourceSystem resources_; // owned fair-rules economy
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
