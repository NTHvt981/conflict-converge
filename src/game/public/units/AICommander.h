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
				TeamId teamID, AIDifficulty difficulty, cc::IVec2 homeAnchorTile,
				cc::IVec2 enemyTile);

	void SetupBase();
	void Reset(AIDifficulty difficulty, cc::IVec2 homeAnchorTile, cc::IVec2 enemyTile,
			   TeamId teamID);
	void Update(float dt);
	void SetOccupancy(OccupancyGrid *occ);
	void SetFog(const FogOfWar *fog);

	TeamId TeamID() const;
	AIDifficulty Difficulty() const;
	int CombatUnitCount() const; // living units excluding Engineers
	int HarvesterCount() const;
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
