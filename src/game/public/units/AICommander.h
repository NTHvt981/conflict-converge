#pragma once

#include "core/Subsystem.h"
#include "units/AICommanderEconomy.h"
#include "units/AICommanderIntel.h"
#include "units/AICommanderTeams.h"

class TileMap;
class OccupancyGrid;
class ResourceNodes;
class EventDispatcher;
class FogOfWar;

class AICommander : public Subsystem
{
public:
	AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes, EventDispatcher &events,
				TeamId teamID, AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile);

	void SetupBase();
	// Restart for a new match: team, difficulty, homes, timers, harvesters,
	// and the owned economy/queue. Reference members are untouched.
	void Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile, TeamId teamID);
	void Update(float dt);
	// Bind shared unit occupancy for footprint-aware orders (null = plain point orders).
	void SetOccupancy(OccupancyGrid *occ);
	// Bind live fog for honest targeting (null = legacy omniscient).
	void SetFog(const FogOfWar *fog);

	TeamId TeamID() const;
	AIDifficulty Difficulty() const;
	int CombatUnitCount() const; // living non-Engineer units on our team
	int HarvesterCount() const;  // living Engineers on our team
	int WavesLaunched() const;
	bool HasScouted() const;
	cc::IVec2 LastSeenEnemy() const;
	bool HasBootcamp() const;
	bool HasWorkshop() const;
	bool HasDepot() const;

private:
	AICommanderIntel intel_;
	AICommanderEconomy economy_;
	AICommanderTeams teams_;
};
