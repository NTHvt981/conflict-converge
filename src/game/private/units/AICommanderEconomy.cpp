#include "units/AICommander.h"

#include "units/Extensions.h"
#include "economy/Building.h"
#include "economy/Nodes.h"
#include "world/Pathfinder.h"

AIDifficultyParams ParamsFor(AIDifficulty difficulty)
{
	AIDifficultyParams params;
	switch (difficulty)
	{
	case AIDifficulty::Easy:
		params.harvesters = 1;
		params.waveThreshold = 3;
		params.scoutInterval = 60.0f;
		params.relaunchCooldown = 30.0f;
		params.retreats = false;
		params.composition = { UnitType::RifleInfantry };
		break;
	case AIDifficulty::Medium:
		params.harvesters = 2;
		params.waveThreshold = 5;
		params.scoutInterval = 30.0f;
		params.relaunchCooldown = 20.0f;
		params.retreats = false;
		params.composition = { UnitType::RifleInfantry, UnitType::RifleInfantry, UnitType::LightTank };
		break;
	case AIDifficulty::Hard:
		params.harvesters = 3;
		params.waveThreshold = 4;
		params.reserveUnits = 2;
		params.scoutInterval = 15.0f;
		params.relaunchCooldown = 15.0f;
		params.retreats = true;
		params.composition = { UnitType::RifleInfantry, UnitType::LightTank, UnitType::Artillery,
							   UnitType::IFV };
		break;
	}
	return params;
}

int AICommander::CombatUnitCount() const
{
	int count = 0;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (unit.teamID == teamID_ && unit.health > 0.0f && unit.type != UnitType::Engineer)
		{
			++count;
		}
	});
	return count;
}

int AICommander::HarvesterCount() const
{
	int count = 0;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (unit.teamID == teamID_ && unit.health > 0.0f && unit.type == UnitType::Engineer)
		{
			++count;
		}
	});
	return count;
}

bool AICommander::HasBootcamp() const
{
	bool found = false;
	registry_.Each<Building>([&](Entity, const Building &building) {
		if (building.teamID == teamID_ && building.type == BuildingType::Bootcamp &&
			building.state == BuildingState::Operational)
		{
			found = true;
		}
	});
	return found;
}

bool AICommander::HasWorkshop() const
{
	bool found = false;
	registry_.Each<Building>([&](Entity, const Building &building) {
		if (building.teamID == teamID_ && building.type == BuildingType::Workshop &&
			building.state == BuildingState::Operational)
		{
			found = true;
		}
	});
	return found;
}

bool AICommander::HasDepot() const
{
	bool found = false;
	registry_.Each<Building>([&](Entity, const Building &building) {
		if (building.teamID == teamID_ && building.type == BuildingType::ResourceDepot &&
			building.state == BuildingState::Operational)
		{
			found = true;
		}
	});
	return found;
}

void AICommander::OrderMove(Unit &unit, Entity id, Vector2 dest)
{
	const OrderContext ctx = MakeOrderContext(registry_, id, map_, occ_);
	if (ctx.occ != nullptr)
	{
		IssuePathOrderFootprint(unit, ctx, dest);
	}
	else
	{
		IssuePathOrder(unit, ctx.orders, ctx.mover, ctx.map, dest);
	}
}

void AICommander::OrderAttackMove(Unit &unit, Entity id, Vector2 dest)
{
	const OrderContext ctx = MakeOrderContext(registry_, id, map_, occ_);
	if (ctx.occ != nullptr)
	{
		IssueAttackMoveOrderFootprint(unit, ctx, dest);
	}
	else
	{
		IssueAttackMoveOrder(unit, ctx.orders, ctx.mover, ctx.map, dest);
	}
}

void AICommander::OrderHarvesterToIron(Entity harvester)
{
	Unit *unit = registry_.Get<Unit>(harvester);
	if (unit == nullptr)
	{
		return;
	}
	Harvester &loop = GetHarvester(registry_, harvester);
	loop.phase = HarvestPhase::ToField;
	loop.load = 0.0f;
	loop.nodeTile = { -1, -1 };
	int best = 0;
	nodes_.Each([&](const ResourceNode &node) {
		if (node.kind != ResourceKind::Iron || node.IsDepleted() ||
			HarvestFieldClaimed(registry_, node.tile, harvester, teamID_))
		{
			return;
		}
		const int dist = (node.tile.x - homeTile_.x) * (node.tile.x - homeTile_.x) +
						 (node.tile.y - homeTile_.y) * (node.tile.y - homeTile_.y);
		if (loop.nodeTile == cc::IVec2{ -1, -1 } || dist < best)
		{
			best = dist;
			loop.nodeTile = node.tile;
		}
	});
	if (!(loop.nodeTile == cc::IVec2{ -1, -1 }))
	{
		OrderMove(*unit, harvester,
				  cc::ToRaylib(cc::TileToWorld(loop.nodeTile.x, loop.nodeTile.y)));
	}
}

void AICommander::MaintainHarvesters()
{
	std::vector<Entity> kept;
	kept.reserve(harvesters_.size());
	for (Entity id : harvesters_)
	{
		const Unit *unit = registry_.Get<Unit>(id);
		if (unit != nullptr && unit->health > 0.0f && unit->teamID == teamID_ &&
			unit->type == UnitType::Engineer)
		{
			kept.push_back(id);
		}
	}
	harvesters_.swap(kept);
	registry_.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.teamID != teamID_ || unit.health <= 0.0f || unit.type != UnitType::Engineer ||
			registry_.Get<Harvester>(id) != nullptr)
		{
			return;
		}
		OrderHarvesterToIron(id);
	});
	if (HasDepot())
	{
		while (static_cast<int>(harvesters_.size()) < params_.harvesters)
		{
			const Entity id = factory_.Spawn(UnitType::Engineer, teamID_,
											 cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
			if (id == kInvalidEntity)
			{
				break;
			}
			OrderHarvesterToIron(id);
			harvesters_.push_back(id);
		}
	}
}

void AICommander::MaintainProduction()
{
	if (params_.composition.empty())
	{
		return;
	}
	const int desired = params_.waveThreshold + 2 + params_.reserveUnits;
	int pipeline = CombatUnitCount() + static_cast<int>(queueBootcamp_.Size()) +
				   static_cast<int>(queueWorkshop_.Size());
	int guard = 0;
	while (pipeline < desired && guard < desired)
	{
		++guard;
		const UnitType type = params_.composition[compIndex_ % params_.composition.size()];
		++compIndex_;
		const bool infantry = ProductionCategoryOf(type) == ProductionCategory::Infantry;
		ProductionQueue &queue = infantry ? queueBootcamp_ : queueWorkshop_;
		if (infantry ? !HasBootcamp() : !HasWorkshop())
		{
			continue;
		}
		if (!queue.Enqueue(resources_, type))
		{
			break;
		}
		++pipeline;
	}
}
