#include "units/AICommander.h"

#include "app/data/AIConfig.h"

AICommander::AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes,
						 EventDispatcher &events, TeamId teamID, AIDifficulty difficulty,
						 cc::IVec2 homeTile, cc::IVec2 enemyTile)
	: intel_(registry), economy_(registry, map, nodes, events), teams_(registry, map, intel_, economy_)
{
	const AIDifficultyParams params = ActiveAIDifficultyParams(difficulty);
	economy_.Reset(teamID, difficulty, homeTile, params);
	teams_.Reset(teamID, params, homeTile, enemyTile);
	intel_.Reset(params.scoutInterval, enemyTile, teamID);
}

void AICommander::Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile,
						TeamId teamID)
{
	const AIDifficultyParams params = ActiveAIDifficultyParams(difficulty);
	economy_.Reset(teamID, difficulty, homeTile, params);
	teams_.Reset(teamID, params, homeTile, enemyTile);
	intel_.Reset(params.scoutInterval, enemyTile, teamID);
}

void AICommander::SetupBase()
{
	economy_.SetupBase();
}

void AICommander::Update(float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}
	economy_.Update(dt);
	intel_.TickCombat();
	intel_.RefreshIntel();
	teams_.Update(dt);
}

void AICommander::SetOccupancy(OccupancyGrid *occ)
{
	economy_.SetOccupancy(occ);
	teams_.SetOccupancy(occ);
}

void AICommander::SetFog(const FogOfWar *fog)
{
	intel_.SetFog(fog);
}

TeamId AICommander::TeamID() const
{
	return economy_.TeamID();
}

AIDifficulty AICommander::Difficulty() const
{
	return economy_.Difficulty();
}

int AICommander::CombatUnitCount() const
{
	return economy_.CombatUnitCount();
}

int AICommander::HarvesterCount() const
{
	return economy_.HarvesterCount();
}

int AICommander::WavesLaunched() const
{
	return teams_.WavesLaunched();
}

bool AICommander::HasScouted() const
{
	return intel_.HasScouted();
}

cc::IVec2 AICommander::LastSeenEnemy() const
{
	return intel_.LastSeenEnemy();
}

bool AICommander::HasBootcamp() const
{
	return economy_.HasBootcamp();
}

bool AICommander::HasWorkshop() const
{
	return economy_.HasWorkshop();
}

bool AICommander::HasDepot() const
{
	return economy_.HasDepot();
}
