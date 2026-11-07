#include "units/AICommanderEconomy.h"

#include "economy/Building.h"
#include "economy/NodeHarvesters.h"
#include "economy/Nodes.h"
#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"

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

AICommanderEconomy::AICommanderEconomy(Registry &registry, TileMap &map, ResourceNodes &nodes,
									   EventDispatcher &events)
	: registry_(registry), map_(map), nodes_(nodes), factory_(registry, resources_, events)
{
}

void AICommanderEconomy::Reset(TeamId teamID, AIDifficulty difficulty, cc::IVec2 homeTile,
							   const AIDifficultyParams &params)
{
	teamID_ = teamID;
	difficulty_ = difficulty;
	homeTile_ = homeTile;
	params_ = params;
	resources_ = ResourceSystem();
	queueBootcamp_.ResetForMatch();
	queueWorkshop_.ResetForMatch();
	harvesters_.clear();
	compIndex_ = 0;
	bootcampCursor_ = 0;
	workshopCursor_ = 0;
}

void AICommanderEconomy::SetOccupancy(OccupancyGrid *occ)
{
	occ_ = occ;
}

TeamId AICommanderEconomy::TeamID() const
{
	return teamID_;
}

AIDifficulty AICommanderEconomy::Difficulty() const
{
	return difficulty_;
}

UnitFactory &AICommanderEconomy::Factory()
{
	return factory_;
}

const AIDifficultyParams &AICommanderEconomy::Params() const
{
	return params_;
}

int AICommanderEconomy::CombatUnitCount() const
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

int AICommanderEconomy::HarvesterCount() const
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

bool AICommanderEconomy::HasBootcamp() const
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

bool AICommanderEconomy::HasWorkshop() const
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

bool AICommanderEconomy::HasDepot() const
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

void AICommanderEconomy::OrderMove(Unit &unit, Entity id, Vector2 dest)
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

void AICommanderEconomy::OrderAttackMove(Unit &unit, Entity id, Vector2 dest)
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

void AICommanderEconomy::OrderHarvesterToIron(Entity harvester)
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
		OrderMove(*unit, harvester, cc::ToRaylib(cc::TileToWorld(loop.nodeTile.x, loop.nodeTile.y)));
	}
}

void AICommanderEconomy::MaintainHarvesters()
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

void AICommanderEconomy::MaintainProduction()
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

void AICommanderEconomy::PumpQueues(float dt)
{
	auto pumpQueue = [&](ProductionQueue &queue, BuildingType type, int &cursor) {
		int producers = 0;
		registry_.Each<Building>([&](Entity, const Building &building) {
			if (building.type == type && building.teamID == teamID_ &&
				building.state == BuildingState::Operational)
			{
				++producers;
			}
		});
		if (producers == 0)
		{
			return;
		}
		const Entity producerId = ProducerBuildingAt(registry_, type, teamID_, cursor);
		if (producerId == kInvalidEntity)
		{
			return;
		}
		const Building *producer = registry_.Get<Building>(producerId);
		if (producer == nullptr)
		{
			return;
		}
		const cc::IVec2 rally = producer->rallyTile;
		const cc::IVec2 spawnTile = BuildingSpawnTile(map_, occ_, *producer);
		const Vector2 spawnPos = cc::ToRaylib(cc::TileToWorld(spawnTile.x, spawnTile.y));
		for (int n = 0; n < producers; ++n)
		{
			const Entity spawned =
				queue.Update(factory_, resources_, teamID_, spawnPos, dt, &map_, occ_);
			if (spawned == kInvalidEntity)
			{
				continue;
			}
			++cursor;
			if (Unit *fresh = registry_.Get<Unit>(spawned))
			{
				if (!(cc::WorldToTile(cc::ToGlm(fresh->position)) == rally))
				{
					OrderMove(*fresh, spawned,
							  cc::ToRaylib(cc::TileToWorld(rally.x, rally.y)));
				}
			}
		}
	};
	if (HasBootcamp())
	{
		pumpQueue(queueBootcamp_, BuildingType::Bootcamp, bootcampCursor_);
	}
	if (HasWorkshop())
	{
		pumpQueue(queueWorkshop_, BuildingType::Workshop, workshopCursor_);
	}
}

void AICommanderEconomy::Update(float dt)
{
	UpdateBaseIncome(registry_, resources_, dt, teamID_);
	nodes_.UpdateHarvesters(registry_, map_, occ_, dt, teamID_);
	nodes_.GatherTick(registry_, resources_, dt, teamID_);
	PumpQueues(dt);
	MaintainHarvesters();
	MaintainProduction();
}

void AICommanderEconomy::SetupBase()
{
	resources_.AddIron(2000);
	resources_.AddOil(600);

	auto placePaid = [&](BuildingType type, int tileX, int tileY) -> Entity {
		const UnitCost cost = BuildingCost(type);
		if (resources_.iron < cost.iron || resources_.oil < cost.oil)
		{
			return kInvalidEntity;
		}
		const Entity id = PlaceBuilding(registry_, map_, type, teamID_, tileX, tileY, &nodes_);
		if (id != kInvalidEntity)
		{
			resources_.TrySpend(cost.iron, cost.oil);
		}
		return id;
	};
	placePaid(BuildingType::Base, homeTile_.x, homeTile_.y);
	const cc::IVec2 depotSpots[] = { { 2, 0 }, { 0, 2 }, { -1, 0 }, { 0, -2 } };
	for (const cc::IVec2 &spot : depotSpots)
	{
		if (placePaid(BuildingType::ResourceDepot, homeTile_.x + spot.x, homeTile_.y + spot.y) !=
			kInvalidEntity)
		{
			break;
		}
	}
	const cc::IVec2 bootcampSpots[] = { { 0, 2 }, { 3, 0 },  { -2, 2 }, { 0, -3 },
										{ -2, 0 }, { 0, -2 }, { 2, -2 }, { -3, 0 } };
	for (const cc::IVec2 &spot : bootcampSpots)
	{
		const Entity id = placePaid(BuildingType::Bootcamp, homeTile_.x + spot.x,
									homeTile_.y + spot.y);
		if (id != kInvalidEntity)
		{
			registry_.Get<Building>(id)->rallyTile = homeTile_ + cc::IVec2(0, 3);
			break;
		}
	}
	const cc::IVec2 workshopSpots[] = { { 3, 2 }, { -3, 3 },  { 0, 5 },  { 0, -4 },
										{ 4, -3 }, { -4, -2 }, { 5, 1 }, { -4, 1 } };
	bool workshopPlaced = false;
	for (const cc::IVec2 &spot : workshopSpots)
	{
		const Entity id = placePaid(BuildingType::Workshop, homeTile_.x + spot.x,
									homeTile_.y + spot.y);
		if (id != kInvalidEntity)
		{
			registry_.Get<Building>(id)->rallyTile = homeTile_ + cc::IVec2(0, 3);
			workshopPlaced = true;
			break;
		}
	}
	for (int ring = 1; !workshopPlaced && ring <= 8; ++ring)
	{
		for (int dy = -ring; !workshopPlaced && dy <= ring; ++dy)
		{
			for (int dx = -ring; !workshopPlaced && dx <= ring; ++dx)
			{
				const Entity id = placePaid(BuildingType::Workshop, homeTile_.x + dx,
											homeTile_.y + dy);
				if (id != kInvalidEntity)
				{
					registry_.Get<Building>(id)->rallyTile = homeTile_ + cc::IVec2(0, 3);
					workshopPlaced = true;
				}
			}
		}
	}
	factory_.Spawn(UnitType::RifleInfantry, teamID_,
				   cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
}
