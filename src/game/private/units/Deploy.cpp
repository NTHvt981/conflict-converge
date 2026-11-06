#include "units/Deploy.h"

#include <algorithm>
#include <cmath>

#include "app/data/Art.h"
#include "economy/Building.h"
#include "units/Extensions.h"
#include "units/Unit.h"
#include "world/Pathfinder.h"

void BeginDeploy(Registry &registry, Entity unitId, Entity producerId, const TileMap &map,
				 OccupancyGrid *occ, int controlGroupBit)
{
	Unit *unit = registry.Get<Unit>(unitId);
	const Building *producer = registry.Get<Building>(producerId);
	if (unit == nullptr || producer == nullptr)
	{
		return;
	}
	const cc::IVec2 door = BuildingSpawnTile(map, occ, *producer);
	unit->position = cc::ToRaylib(cc::TileToWorld(door.x, door.y));

	Deployment deployment;
	deployment.producer = producerId;
	deployment.groupBit = controlGroupBit;
	deployment.exitDelay = kDeployExitDelayPerMember;
	deployment.doorTile = door;
	const SquadLayout layout =
		SquadSlots(unit->type, static_cast<unsigned int>(unitId), 1.0f);
	deployment.membersTotal = layout.count;
	const cc::Vec2 origin = cc::ToGlm(unit->position);
	deployment.memberSpawnedPos.assign(static_cast<std::size_t>(deployment.membersTotal),
									   origin);
	deployment.memberSlotPos.reserve(static_cast<std::size_t>(deployment.membersTotal));
	deployment.memberParked.assign(static_cast<std::size_t>(deployment.membersTotal), false);
	for (int i = 0; i < deployment.membersTotal; ++i)
	{
		deployment.memberSlotPos.push_back(
			origin + cc::Vec2{ layout.slots[i].x, layout.slots[i].y });
	}
	registry.Add(unitId, std::move(deployment));
}

void UpdateDeploy(Registry &registry, const TileMap &map, OccupancyGrid *occ, float dt)
{
	(void)map;
	if (dt <= 0.0f)
	{
		return;
	}
	std::vector<Entity> deploying;
	registry.Each<Deployment>(
		[&](Entity id, const Deployment &) { deploying.push_back(id); });
	for (Entity id : deploying)
	{
		Deployment *deployment = registry.Get<Deployment>(id);
		Unit *unit = registry.Get<Unit>(id);
		if (deployment == nullptr || unit == nullptr)
		{
			continue;
		}
		deployment->nextSpawnAt += dt;
		while (deployment->membersSpawned < deployment->membersTotal &&
			   deployment->nextSpawnAt >= deployment->exitDelay *
										   static_cast<float>(deployment->membersSpawned + 1))
		{
			deployment->memberSpawnedPos[static_cast<std::size_t>(deployment->membersSpawned)] =
				cc::ToGlm(unit->position);
			++deployment->membersSpawned;
		}

		const float step = unit->speed * kDeployWalkSpeedMultiplier * dt;
		bool allParked = deployment->membersSpawned == deployment->membersTotal;
		for (int i = 0; i < deployment->membersSpawned; ++i)
		{
			cc::Vec2 &pos = deployment->memberSpawnedPos[static_cast<std::size_t>(i)];
			const cc::Vec2 target = deployment->memberSlotPos[static_cast<std::size_t>(i)];
			if (deployment->memberParked[static_cast<std::size_t>(i)])
			{
				pos = target;
				continue;
			}
			const cc::Vec2 delta = target - pos;
			const float distance = glm::length(delta);
			if (distance <= kDeploySlotParkRadius || distance <= step)
			{
				pos = target;
				deployment->memberParked[static_cast<std::size_t>(i)] = true;
				continue;
			}
			allParked = false;
			pos += delta / distance * step;
		}
		if (allParked)
		{
			AssembleDeploy(registry, map, occ, id);
		}
	}
}

void AssembleDeploy(Registry &registry, const TileMap &map, OccupancyGrid *occ, Entity unitId)
{
	Unit *unit = registry.Get<Unit>(unitId);
	Deployment *deployment = registry.Get<Deployment>(unitId);
	if (unit == nullptr || deployment == nullptr)
	{
		return;
	}
	const cc::IVec2 door = deployment->doorTile;
	const Entity producerId = deployment->producer;
	const int groupBit = deployment->groupBit;
	unit->position = cc::ToRaylib(cc::TileToWorld(door.x, door.y));
	registry.Remove<Deployment>(unitId);
	if (occ != nullptr)
	{
		(void)occ->ReserveFootprintOwned(door, unit->footprintWidth, unit->footprintHeight,
										 unitId, registry.Generation(unitId));
	}
	if (const Building *producer = registry.Get<Building>(producerId))
	{
		const cc::IVec2 rally = producer->rallyTile;
		if (!(cc::WorldToTile(cc::ToGlm(unit->position)) == rally))
		{
			const Vector2 rallyPos = cc::ToRaylib(cc::TileToWorld(rally.x, rally.y));
			const OrderContext ctx = MakeOrderContext(registry, unitId, map, occ);
			if (occ != nullptr)
			{
				IssuePathOrderFootprint(*unit, ctx, rallyPos);
			}
			else
			{
				IssuePathOrder(*unit, ctx.orders, ctx.mover, ctx.map, rallyPos);
			}
		}
	}
	if (groupBit >= 0)
	{
		unit->controlGroups |= (1u << static_cast<unsigned int>(groupBit));
	}
}
