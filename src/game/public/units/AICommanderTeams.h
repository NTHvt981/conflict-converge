#pragma once

#include <cstddef>
#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"
#include "units/AICommanderEconomy.h"
#include "units/Targeting.h"

class TileMap;
class OccupancyGrid;
class AICommanderIntel;

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

class AICommanderTeams
{
public:
	AICommanderTeams(Registry &registry, TileMap &map, AICommanderIntel &intel,
					 AICommanderEconomy &economy);

	void Reset(TeamId teamID, const AIDifficultyParams &params, cc::IVec2 homeTile,
			   cc::IVec2 enemyTile);
	void SetOccupancy(OccupancyGrid *occ);
	void Update(float dt);
	int WavesLaunched() const;

private:
	void MaybeLaunchWave();
	void MusterTeams();
	std::vector<Entity> GatherUnassignedUnits();
	void EnsureGuardTeam(std::vector<Entity> &unassigned);
	void EnsureHarassTeam(std::vector<Entity> &unassigned);
	void AssignRemainingToMain(std::vector<Entity> &unassigned);
	void LaunchTeams();
	void UpdateTeams(float dt);
	void UpdateGuardPost();
	void IssueTeamStep(AITeam &team);
	bool TeamAllIdle(const AITeam &team);
	void PruneTeam(AITeam &team);
	cc::IVec2 SelectTarget();
	cc::IVec2 SelectTarget(ThreatScope scope);
	int Escalation() const;
	int EffectiveThreshold() const;
	float EffectiveCooldown() const;
	void ScoutTick(float dt);
	void RetreatTick();

	Registry &registry_;
	TileMap &map_;
	AICommanderIntel &intel_;
	AICommanderEconomy &economy_;
	OccupancyGrid *occ_ = nullptr;
	TeamId teamID_ = kEnemyTeam;
	cc::IVec2 homeTile_{ 0, 0 };
	cc::IVec2 enemyTile_{ 0, 0 };
	cc::IVec2 rallyTile_{ 0, 0 };
	float timeSinceLaunch_ = 0.0f;
	float matchTime_ = 0.0f;
	int wavesLaunched_ = 0;
	std::vector<AITeam> teams_;
};
