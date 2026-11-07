#include "units/AICommander.h"

#include "app/data/AIConfig.h"
#include "economy/Building.h"
#include "economy/Nodes.h"
#include "world/FogOfWar.h"
#include "world/TileMap.h"

AICommander::AICommander(Registry &registry, TileMap &map, ResourceNodes &nodes,
						 EventDispatcher &events, TeamId teamID, AIDifficulty difficulty,
						 cc::IVec2 homeTile, cc::IVec2 enemyTile)
	: registry_(registry), map_(map), nodes_(nodes), factory_(registry, resources_, events),
	  params_(ActiveAIDifficultyParams(difficulty)), teamID_(teamID), difficulty_(difficulty), homeTile_(homeTile),
	  enemyTile_(enemyTile), rallyTile_(homeTile + cc::IVec2(0, 3)), lastSeenEnemy_(enemyTile)
{
	scoutTimer_ = params_.scoutInterval;
	timeSinceLaunch_ = params_.relaunchCooldown;
}

TeamId AICommander::TeamID() const
{
	return teamID_;
}

AIDifficulty AICommander::Difficulty() const
{
	return difficulty_;
}

int AICommander::WavesLaunched() const
{
	return wavesLaunched_;
}

void AICommander::Reset(AIDifficulty difficulty, cc::IVec2 homeTile, cc::IVec2 enemyTile,
						TeamId teamID)
{
	params_ = ActiveAIDifficultyParams(difficulty);
	difficulty_ = difficulty;
	teamID_ = teamID;
	homeTile_ = homeTile;
	enemyTile_ = enemyTile;
	rallyTile_ = homeTile + cc::IVec2(0, 3);
	lastSeenEnemy_ = enemyTile;
	resources_ = ResourceSystem();
	queueBootcamp_.ResetForMatch();
	queueWorkshop_.ResetForMatch();
	harvesters_.clear();
	teams_.clear();
	compIndex_ = 0;
	bootcampCursor_ = 0;
	workshopCursor_ = 0;
	scouted_ = false;
	scoutTimer_ = params_.scoutInterval;
	timeSinceLaunch_ = params_.relaunchCooldown;
	matchTime_ = 0.0f;
	losses_ = 0;
	liveCombat_ = -1;
	contact_ = false;
	wavesLaunched_ = 0;
}

void AICommander::SetupBase()
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
			registry_.Get<Building>(id)->rallyTile = rallyTile_;
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
			registry_.Get<Building>(id)->rallyTile = rallyTile_;
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
					registry_.Get<Building>(id)->rallyTile = rallyTile_;
					workshopPlaced = true;
				}
			}
		}
	}
	factory_.Spawn(UnitType::RifleInfantry, teamID_,
				   cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
}

void AICommander::SetOccupancy(OccupancyGrid *occ)
{
	occ_ = occ;
}

void AICommander::SetFog(const FogOfWar *fog)
{
	fog_ = fog;
}

void AICommander::Update(float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}
	UpdateBaseIncome(registry_, resources_, dt, teamID_);
	nodes_.UpdateHarvesters(registry_, map_, occ_, dt, teamID_);
	nodes_.GatherTick(registry_, resources_, dt, teamID_);
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
	MaintainHarvesters();
	MaintainProduction();
	timeSinceLaunch_ += dt;
	matchTime_ += dt;
	int liveCombat = 0;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (unit.teamID == teamID_ && unit.health > 0.0f && unit.type != UnitType::Engineer)
		{
			++liveCombat;
			if (unit.state == UnitState::Attacking)
			{
				contact_ = true;
			}
		}
	});
	if (liveCombat_ >= 0 && liveCombat < liveCombat_)
	{
		losses_ += liveCombat_ - liveCombat;
	}
	liveCombat_ = liveCombat;
	RefreshIntel();
	MaybeLaunchWave();
	UpdateTeams(dt);
	UpdateGuardPost();
	ScoutTick(dt);
	if (params_.retreats)
	{
		RetreatTick();
	}
}
