#pragma once

#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"

class ResourceSystem;
class TileMap;
class OccupancyGrid;
struct ResourceNode;

class NodeHarvesters
{
public:
	void GatherTick(std::vector<ResourceNode> &nodes, Registry &registry,
					ResourceSystem &resources, float dt, TeamId teamID);
	// Drives loop harvesters between fields and depots; engineers without a
	// Harvester component keep the legacy stand-and-trickle behavior.
	void UpdateHarvesters(std::vector<ResourceNode> &nodes, Registry &registry,
						  const TileMap &map, const OccupancyGrid *occ, float dt,
						  TeamId teamID);

	float IronCarry() const;
	float OilCarry() const;
	void SetCarry(float ironCarry, float oilCarry);

private:
	float ironCarry_ = 0.0f;
	float oilCarry_ = 0.0f;
};

// True when another live unit of the team works the tile.
bool HarvestFieldClaimed(const Registry &registry, cc::IVec2 tile, Entity self, TeamId teamID);
