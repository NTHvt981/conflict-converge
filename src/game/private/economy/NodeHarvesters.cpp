#include "economy/NodeHarvesters.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "units/Unit.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"

inline constexpr float kGatherIronPerSecond = 10.0f;
inline constexpr float kGatherOilPerSecond = 8.0f;
inline constexpr float kLoadIronPerSecond = 16.0f;

namespace
{

const ResourceNode *FindNodeAt(const std::vector<ResourceNode> &nodes, cc::IVec2 tile)
{
	for (const ResourceNode &node : nodes)
	{
		if (node.tile == tile)
		{
			return &node;
		}
	}
	return nullptr;
}

bool OnDepotEntrance(const Registry &registry, cc::IVec2 tile, TeamId teamID)
{
	return registry.Any<Building>([&](Entity, const Building &building) {
		if (building.teamID != teamID || building.state != BuildingState::Operational ||
			building.type != BuildingType::ResourceDepot)
		{
			return false;
		}
		const cc::IVec2 fp = Footprint(building.type);
		for (int dy = -1; dy <= fp.y; ++dy)
		{
			for (int dx = -1; dx <= fp.x; ++dx)
			{
				if (dx >= 0 && dx < fp.x && dy >= 0 && dy < fp.y)
				{
					continue;
				}
				if (tile == cc::IVec2(building.tileX + dx, building.tileY + dy))
				{
					return true;
				}
			}
		}
		return false;
	});
}

cc::IVec2 NearestDepotEntrance(const Registry &registry, const TileMap &map, cc::IVec2 from,
							   TeamId teamID)
{
	cc::IVec2 best{ -1, -1 };
	int bestDist = 0;
	bool have = false;
	registry.Each<Building>([&](Entity, const Building &building) {
		if (building.teamID != teamID || building.state != BuildingState::Operational ||
			building.type != BuildingType::ResourceDepot)
		{
			return;
		}
		const cc::IVec2 fp = Footprint(building.type);
		for (int dy = -1; dy <= fp.y; ++dy)
		{
			for (int dx = -1; dx <= fp.x; ++dx)
			{
				if (dx >= 0 && dx < fp.x && dy >= 0 && dy < fp.y)
				{
					continue;
				}
				const cc::IVec2 tile{ building.tileX + dx, building.tileY + dy };
				if (!map.InBounds(tile) || map.IsBlocked(tile))
				{
					continue;
				}
				const int dist = (tile.x - from.x) * (tile.x - from.x) +
								 (tile.y - from.y) * (tile.y - from.y);
				if (!have || dist < bestDist)
				{
					have = true;
					bestDist = dist;
					best = tile;
				}
			}
		}
	});
	return best;
}

void OrderHarvestMove(Registry &registry, Unit &unit, Entity id, const TileMap &map,
					  const OccupancyGrid *occ, Vector2 dest)
{
	if (occ != nullptr)
	{
		IssuePathOrderFootprint(unit, MakeOrderContext(registry, id, map, occ), dest);
	}
	else
	{
		IssuePathOrder(unit, GetOrders(registry, id), GetMover(registry, id), map, dest);
	}
}

bool EligibleEngineer(const Unit &unit, TeamId teamID)
{
	return unit.type == UnitType::Engineer && unit.health > 0.0f &&
		   (teamID == kAnyTeam || unit.teamID == teamID);
}

void LoadFromNode(std::vector<ResourceNode> &nodes, Harvester &harvester, const Unit &unit,
				  cc::IVec2 tile, float dt)
{
	for (ResourceNode &node : nodes)
	{
		if (!(node.tile == tile) || node.IsDepleted())
		{
			continue;
		}
		const float rate =
			node.kind == ResourceKind::Iron ? kLoadIronPerSecond : kGatherOilPerSecond;
		float take = rate * dt;
		if (take > node.amount)
		{
			take = node.amount;
		}
		if (take > harvester.capacity - harvester.load)
		{
			take = harvester.capacity - harvester.load;
		}
		if (take <= 0.0f)
		{
			break;
		}
		node.amount -= take;
		if (node.IsDepleted())
		{
			node.respawnTimer = node.respawnDelay;
		}
		harvester.load += take;
		harvester.loadKind = node.kind;
		if (harvester.load >= harvester.capacity)
		{
			harvester.phase = HarvestPhase::ToDepot;
			harvester.nodeTile = { -1, -1 };
		}
		break;
	}
}

void UnloadAtDepot(Registry &registry, ResourceSystem &resources, Harvester &harvester,
				   const Unit &unit)
{
	const long whole = static_cast<long>(harvester.load);
	long stored = 0;
	if (harvester.loadKind == ResourceKind::Oil)
	{
		stored = DepositOil(registry, resources, unit.teamID, whole);
	}
	else
	{
		stored = DepositIron(registry, resources, unit.teamID, whole);
	}
	harvester.load -= static_cast<float>(stored);
	if (harvester.load <= 0.0f)
	{
		harvester.load = 0.0f;
		harvester.phase = HarvestPhase::ToField;
		harvester.nodeTile = { -1, -1 };
	}
}

void TrickleIntoNode(std::vector<ResourceNode> &nodes, ResourceSystem &resources,
					 float &ironCarry, float &oilCarry, cc::IVec2 tile, float dt)
{
	for (ResourceNode &node : nodes)
	{
		if (node.tile != tile || node.IsDepleted())
		{
			continue;
		}
		const float rate =
			node.kind == ResourceKind::Iron ? kGatherIronPerSecond : kGatherOilPerSecond;
		float &carry = node.kind == ResourceKind::Iron ? ironCarry : oilCarry;
		float take = rate * dt;
		if (take > node.amount)
		{
			take = node.amount;
		}
		node.amount -= take;
		if (node.IsDepleted())
		{
			node.respawnTimer = node.respawnDelay;
		}
		carry += take;
		const long whole = static_cast<long>(carry);
		if (node.kind == ResourceKind::Iron)
		{
			resources.AddIron(whole);
		}
		else
		{
			resources.AddOil(whole);
		}
		carry -= static_cast<float>(whole);
		break;
	}
}

void HandleToField(std::vector<ResourceNode> &nodes, Registry &registry, const TileMap &map,
				   const OccupancyGrid *occ, Entity id, Unit &unit, Harvester &harvester,
				   bool idle, cc::IVec2 here)
{
	const ResourceNode *node = FindNodeAt(nodes, harvester.nodeTile);
	const bool live = node != nullptr && !node->IsDepleted();
	const bool taken = live && here == harvester.nodeTile &&
					   HarvestFieldClaimed(registry, harvester.nodeTile, id, unit.teamID);
	if (live && here == harvester.nodeTile && !taken)
	{
		harvester.phase = HarvestPhase::Loading;
		return;
	}
	if (!live || taken)
	{
		if (taken)
		{
			harvester.nodeTile = { -1, -1 };
		}
		const ResourceNode *best = nullptr;
		int bestDist = 0;
		for (const ResourceNode &candidate : nodes)
		{
			if (candidate.IsDepleted() ||
				HarvestFieldClaimed(registry, candidate.tile, id, unit.teamID))
			{
				continue;
			}
			const int dist = (candidate.tile.x - here.x) * (candidate.tile.x - here.x) +
							 (candidate.tile.y - here.y) * (candidate.tile.y - here.y);
			if (best == nullptr || dist < bestDist)
			{
				best = &candidate;
				bestDist = dist;
			}
		}
		if (best == nullptr)
		{
			harvester.nodeTile = { -1, -1 };
			return;
		}
		harvester.nodeTile = best->tile;
	}
	if (idle && !(here == harvester.nodeTile))
	{
		OrderHarvestMove(registry, unit, id, map, occ,
						 cc::ToRaylib(
							 cc::TileToWorld(harvester.nodeTile.x, harvester.nodeTile.y)));
	}
}

void HandleLoading(std::vector<ResourceNode> &nodes, Harvester &harvester, cc::IVec2 here)
{
	if (harvester.load >= harvester.capacity)
	{
		harvester.phase = HarvestPhase::ToDepot;
		harvester.nodeTile = { -1, -1 };
		return;
	}
	const ResourceNode *node = FindNodeAt(nodes, harvester.nodeTile);
	if (node == nullptr || node->IsDepleted() || !(here == harvester.nodeTile))
	{
		harvester.phase = HarvestPhase::ToField;
	}
}

void HandleToDepot(Registry &registry, const TileMap &map, const OccupancyGrid *occ, Entity id,
				   Unit &unit, Harvester &harvester, bool idle, cc::IVec2 here)
{
	if (OnDepotEntrance(registry, here, unit.teamID))
	{
		harvester.phase = HarvestPhase::Unloading;
		return;
	}
	if (!idle)
	{
		return;
	}
	const cc::IVec2 entrance = NearestDepotEntrance(registry, map, here, unit.teamID);
	if (entrance.x < 0)
	{
		return;
	}
	OrderHarvestMove(registry, unit, id, map, occ,
					 cc::ToRaylib(cc::TileToWorld(entrance.x, entrance.y)));
}

void HandleUnloading(Harvester &harvester, Registry &registry, cc::IVec2 here, TeamId teamID)
{
	if (harvester.load <= 0.0f)
	{
		harvester.phase = HarvestPhase::ToField;
		harvester.nodeTile = { -1, -1 };
	}
	else if (!OnDepotEntrance(registry, here, teamID))
	{
		harvester.phase = HarvestPhase::ToDepot;
	}
}

void TickHarvester(std::vector<ResourceNode> &nodes, Registry &registry,
				   ResourceSystem &resources, Entity id, const Unit &unit, float dt)
{
	Harvester *harvester = registry.Get<Harvester>(id);
	if (harvester == nullptr)
	{
		return;
	}
	const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
	if (harvester->phase == HarvestPhase::Loading && tile == harvester->nodeTile)
	{
		LoadFromNode(nodes, *harvester, unit, tile, dt);
	}
	else if (harvester->phase == HarvestPhase::Unloading &&
			 OnDepotEntrance(registry, tile, unit.teamID))
	{
		UnloadAtDepot(registry, resources, *harvester, unit);
	}
}

void StepHarvester(std::vector<ResourceNode> &nodes, Registry &registry, const TileMap &map,
				   const OccupancyGrid *occ, Entity id, Unit &unit, float dt, TeamId teamID)
{
	Harvester *harvester = registry.Get<Harvester>(id);
	if (harvester == nullptr)
	{
		return;
	}
	const Mover *mover = registry.Get<Mover>(id);
	const bool idle = mover == nullptr || (!mover->hasMoveOrder && !mover->hasPath);
	const cc::IVec2 here = cc::WorldToTile(cc::ToGlm(unit.position));
	switch (harvester->phase)
	{
	case HarvestPhase::ToField:
		HandleToField(nodes, registry, map, occ, id, unit, *harvester, idle, here);
		break;
	case HarvestPhase::Loading:
		HandleLoading(nodes, *harvester, here);
		break;
	case HarvestPhase::ToDepot:
		HandleToDepot(registry, map, occ, id, unit, *harvester, idle, here);
		break;
	case HarvestPhase::Unloading:
		HandleUnloading(*harvester, registry, here, unit.teamID);
		break;
	}
}

} // namespace

bool HarvestFieldClaimed(const Registry &registry, cc::IVec2 tile, Entity self, TeamId teamID)
{
	return registry.Any<Unit>([&](Entity id, const Unit &unit) {
		if (id == self || unit.teamID != teamID || unit.health <= 0.0f)
		{
			return false;
		}
		const Harvester *other = registry.Get<Harvester>(id);
		return other != nullptr && other->nodeTile == tile;
	});
}

void NodeHarvesters::GatherTick(std::vector<ResourceNode> &nodes, Registry &registry,
								ResourceSystem &resources, float dt, TeamId teamID)
{
	if (dt <= 0.0f)
	{
		return;
	}
	registry.Each<Unit>([&](Entity id, const Unit &unit) {
		if (!EligibleEngineer(unit, teamID))
		{
			return;
		}
		Harvester *harvester = registry.Get<Harvester>(id);
		if (harvester != nullptr)
		{
			TickHarvester(nodes, registry, resources, id, unit, dt);
			return;
		}
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
		TrickleIntoNode(nodes, resources, ironCarry_, oilCarry_, tile, dt);
	});
}

void NodeHarvesters::UpdateHarvesters(std::vector<ResourceNode> &nodes, Registry &registry,
									  const TileMap &map, const OccupancyGrid *occ, float dt,
									  TeamId teamID)
{
	if (dt <= 0.0f)
	{
		return;
	}
	registry.Each<Unit>([&](Entity id, Unit &unit) {
		if (!EligibleEngineer(unit, teamID))
		{
			return;
		}
		StepHarvester(nodes, registry, map, occ, id, unit, dt, teamID);
	});
}

float NodeHarvesters::IronCarry() const
{
	return ironCarry_;
}

float NodeHarvesters::OilCarry() const
{
	return oilCarry_;
}

void NodeHarvesters::SetCarry(float ironCarry, float oilCarry)
{
	ironCarry_ = ironCarry;
	oilCarry_ = oilCarry;
}
