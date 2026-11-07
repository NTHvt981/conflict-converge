#pragma once

#include <cstddef>
#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"
#include "economy/Production.h"
#include "economy/ResourceSystem.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

class TileMap;
class OccupancyGrid;
class ResourceNodes;
class EventDispatcher;

enum class AIDifficulty
{
	Easy,
	Medium,
	Hard
};

struct AIDifficultyParams
{
	int harvesters = 2;                // Engineers to sustain on iron
	int waveThreshold = 5;             // combat units required to launch a wave
	int reserveUnits = 0;              // extra standing army above threshold+2
	float scoutInterval = 30.0f;       // seconds between scout dispatches
	float relaunchCooldown = 20.0f;    // seconds between wave launches
	bool retreats = false;             // pull sub-30% units home (Hard only)
	std::vector<UnitType> composition; // cycled when filling the pipeline
};

AIDifficultyParams ParamsFor(AIDifficulty difficulty);

class AICommanderEconomy
{
public:
	AICommanderEconomy(Registry &registry, TileMap &map, ResourceNodes &nodes,
					   EventDispatcher &events);

	void Reset(TeamId teamID, AIDifficulty difficulty, cc::IVec2 homeTile,
			   const AIDifficultyParams &params);
	void SetOccupancy(OccupancyGrid *occ);
	void Update(float dt);
	void SetupBase();

	TeamId TeamID() const;
	AIDifficulty Difficulty() const;
	UnitFactory &Factory();
	const AIDifficultyParams &Params() const;
	int CombatUnitCount() const; // living non-Engineer units on our team
	int HarvesterCount() const;  // living Engineers on our team
	bool HasBootcamp() const;
	bool HasWorkshop() const;
	bool HasDepot() const;

	void OrderMove(Unit &unit, Entity id, Vector2 dest);
	void OrderAttackMove(Unit &unit, Entity id, Vector2 dest);
	void OrderHarvesterToIron(Entity harvester);

private:
	void MaintainHarvesters();
	void MaintainProduction();
	void PumpQueues(float dt);

	Registry &registry_;
	TileMap &map_;
	ResourceNodes &nodes_;
	OccupancyGrid *occ_ = nullptr;
	ResourceSystem resources_; // owned fair-rules economy
	UnitFactory factory_;
	ProductionQueue queueBootcamp_{ ProductionCategory::Infantry };
	ProductionQueue queueWorkshop_{ ProductionCategory::Vehicle };
	AIDifficultyParams params_;
	TeamId teamID_ = kEnemyTeam;
	AIDifficulty difficulty_ = AIDifficulty::Medium;
	cc::IVec2 homeTile_{ 0, 0 };
	int bootcampCursor_ = 0;
	int workshopCursor_ = 0;
	std::size_t compIndex_ = 0;
	std::vector<Entity> harvesters_; // tracked for replacement
};
