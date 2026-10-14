#pragma once

#include <cstddef>
#include <vector>

#include "core/MathUtils.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "core/Subsystem.h"
#include "units/Targeting.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

class TileMap;
class OccupancyGrid;
class ResourceNodes;
class EventDispatcher;
class FogOfWar;

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

enum class AITeamRole
{
	Main,
	Harass,
	Guard
};

enum class AIMissionKind
{
	Move,
	AttackMove,
	Guard
};

struct AIMissionStep
{
	AIMissionKind kind = AIMissionKind::AttackMove;
	cc::IVec2 target{ 0, 0 };
	float timeout = 120.0f;
};

struct AITeam
{
	AITeamRole role = AITeamRole::Main;
	std::vector<Entity> members;
	std::vector<AIMissionStep> script;
	std::size_t step = 0;
	float stepTime = 0.0f;
	std::size_t peakSize = 0;
	bool regrouping = false;
	bool active = false;
};

class AICommander : public Subsystem
{
public:
	AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes, EventDispatcher &events,
				int teamID, AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile);

	void SetupBase();
	// Restart for a new match: team, difficulty, homes, timers, harvesters,
	// and the owned economy/queue. Reference members are untouched.
	void Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile, int teamID);
	void Update(float dt);
	// Bind shared unit occupancy for footprint-aware orders (null = plain point orders).
	void SetOccupancy(OccupancyGrid *occ);
	// Bind live fog for honest targeting (null = legacy omniscient).
	void SetFog(const FogOfWar *fog);

	int TeamID() const;
	AIDifficulty Difficulty() const;
	int CombatUnitCount() const;   // living non-Engineer units on our team
	int HarvesterCount() const;    // living Engineers on our team
	int WavesLaunched() const;
	bool HasScouted() const;
	cc::IVec2 LastSeenEnemy() const;
	bool HasBootcamp() const;
	bool HasWorkshop() const;

private:
	void MaintainHarvesters();
	void MaintainProduction();
	void MaybeLaunchWave();
	void MusterTeams();
	void LaunchTeams();
	void UpdateTeams(float dt);
	void UpdateGuardPost();
	void IssueTeamStep(AITeam &team);
	void OrderAttackMove(Unit &unit, Entity id, Vector2 dest);
	bool TeamAllIdle(const AITeam &team);
	void PruneTeam(AITeam &team);
	cc::IVec2 SelectTarget();
	cc::IVec2 SelectTarget(ThreatScope scope);
	// Fog-gated visibility: omniscient when no fog is bound.
	bool Sees(cc::IVec2 tile) const;
	bool EnemyForceSighted() const;
	void RefreshIntel();
	int Escalation() const;
	int EffectiveThreshold() const;
	float EffectiveCooldown() const;
	void ScoutTick(float dt);
	void RetreatTick();
	void OrderHarvesterToIron(Entity harvester);
	void OrderMove(Unit &unit, Entity id, Vector2 dest);

	Registry &registry_;
	TileMap &map_;
	ResourceNodes &nodes_;
	OccupancyGrid *occ_ = nullptr;
	const FogOfWar *fog_ = nullptr;
	ResourceSystem resources_; // owned fair-rules economy
	UnitFactory factory_;
	ProductionQueue queueBootcamp_{ ProductionCategory::Infantry };
	ProductionQueue queueWorkshop_{ ProductionCategory::Vehicle };
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
	float matchTime_ = 0.0f;
	int losses_ = 0;
	int liveCombat_ = -1;
	bool contact_ = false;
	int wavesLaunched_ = 0;
	int bootcampCursor_ = 0;
	int workshopCursor_ = 0;
	std::size_t compIndex_ = 0;
	std::vector<Entity> harvesters_; // tracked for replacement
	std::vector<AITeam> teams_;
};
