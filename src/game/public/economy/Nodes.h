#pragma once

#include <functional>
#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"
#include "economy/ResourceSystem.h"
#include "core/Subsystem.h"

class TileMap;
class OccupancyGrid;

enum class ResourceKind
{
	Iron,
	Oil,
	Count
};

struct ResourceNode
{
	ResourceKind kind = ResourceKind::Iron;
	cc::IVec2 tile{ 0, 0 };
	float amount = 0.0f;
	float maxAmount = 0.0f;
	float respawnDelay = 0.0f; // seconds from depletion to refill
	float respawnTimer = 0.0f;

	bool IsDepleted() const;
};

class ResourceNodes : public Subsystem
{
public:
	// False leaves storage untouched. Negative respawnDelay exhausts forever.
	bool SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile, float amount,
				   float respawnDelay);
	void Update(float dt);
	void GatherTick(Registry &registry, ResourceSystem &resources, float dt,
					TeamId teamID = kAnyTeam);
	// Drives loop harvesters between fields and depots; engineers without a
	// Harvester component keep the legacy stand-and-trickle behavior.
	void UpdateHarvesters(Registry &registry, const TileMap &map, const OccupancyGrid *occ,
						  float dt, TeamId teamID = kAnyTeam);

	std::size_t Count() const;
	const ResourceNode *FindAt(cc::IVec2 tile) const;
	void Each(const std::function<void(const ResourceNode &)> &fn) const;

	// Save/load: restore a node verbatim (loader already validated the map).
	void RestoreNode(ResourceKind kind, cc::IVec2 tile, float amount, float maxAmount,
					 float respawnDelay, float respawnTimer);
	float IronCarry() const;
	float OilCarry() const;
	void SetCarry(float ironCarry, float oilCarry);

private:
	std::vector<ResourceNode> nodes_;
	float ironCarry_ = 0.0f;
	float oilCarry_ = 0.0f;
};

// True when another live unit of the team works the tile.
bool HarvestFieldClaimed(const Registry &registry, cc::IVec2 tile, Entity self, TeamId teamID);
