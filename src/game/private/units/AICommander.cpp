#include "units/AICommander.h"

#include <algorithm>

#include "app/data/AIConfig.h"
#include "economy/Building.h"
#include "units/Extensions.h"
#include "units/Formation.h"
#include "economy/Nodes.h"
#include "world/FogOfWar.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/UnitStats.h"

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

int AICommander::WavesLaunched() const
{
	return wavesLaunched_;
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

bool AICommander::HasScouted() const
{
	return scouted_;
}

cc::IVec2 AICommander::LastSeenEnemy() const
{
	return lastSeenEnemy_;
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

bool AICommander::Sees(cc::IVec2 tile) const
{
	return fog_ == nullptr || fog_->IsVisible(teamID_, tile);
}

bool AICommander::EnemyForceSighted() const
{
	bool sighted = false;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (sighted || unit.teamID == teamID_ || unit.health <= 0.0f)
		{
			return;
		}
		if (Sees(cc::WorldToTile(cc::ToGlm(unit.position))))
		{
			sighted = true;
		}
	});
	return sighted;
}

void AICommander::RefreshIntel()
{
	if (fog_ == nullptr)
	{
		return;
	}
	long sumX = 0;
	long sumY = 0;
	int count = 0;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (unit.teamID == teamID_ || unit.health <= 0.0f)
		{
			return;
		}
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
		if (!Sees(tile))
		{
			return;
		}
		sumX += tile.x;
		sumY += tile.y;
		++count;
	});
	if (count > 0)
	{
		lastSeenEnemy_ = { static_cast<int>(sumX / count), static_cast<int>(sumY / count) };
	}
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
	std::vector<Entity> alive;
	alive.reserve(harvesters_.size());
	for (Entity id : harvesters_)
	{
		const Unit *unit = registry_.Get<Unit>(id);
		if (unit != nullptr && unit->health > 0.0f)
		{
			alive.push_back(id);
		}
	}
	harvesters_.swap(alive);
	registry_.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.teamID != teamID_ || unit.health <= 0.0f || unit.type != UnitType::Engineer ||
			FindHarvester(registry_, id) != nullptr)
		{
			return;
		}
		OrderHarvesterToIron(id);
	});
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

void AICommander::PruneTeam(AITeam &team)
{
	std::vector<Entity> alive;
	alive.reserve(team.members.size());
	for (Entity id : team.members)
	{
		const Unit *unit = registry_.Get<Unit>(id);
		if (unit != nullptr && unit->health > 0.0f && unit->teamID == teamID_)
		{
			alive.push_back(id);
		}
	}
	team.members.swap(alive);
}

bool AICommander::TeamAllIdle(const AITeam &team)
{
	for (Entity id : team.members)
	{
		const Mover *mover = FindMover(registry_, id);
		if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
		{
			return false;
		}
	}
	return true;
}

void AICommander::IssueTeamStep(AITeam &team)
{
	if (team.step >= team.script.size())
	{
		return;
	}
	const AIMissionStep &mission = team.script[team.step];
	const std::vector<cc::IVec2> offsets = formation::FormationOffsets(team.members.size());
	for (std::size_t i = 0; i < team.members.size(); ++i)
	{
		Unit *unit = registry_.Get<Unit>(team.members[i]);
		if (unit == nullptr)
		{
			continue;
		}
		const cc::IVec2 slot = mission.target + offsets[i];
		const Vector2 dest = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));
		switch (mission.kind)
		{
		case AIMissionKind::Move:
			OrderMove(*unit, team.members[i], dest);
			break;
		case AIMissionKind::Guard:
			OrderMove(*unit, team.members[i], dest);
			if (CombatState *combat = registry_.Get<CombatState>(team.members[i]))
			{
				SetStance(*unit, GetOrders(registry_, team.members[i]), *combat, Stance::Guard);
			}
			break;
		case AIMissionKind::AttackMove:
		default:
			OrderAttackMove(*unit, team.members[i], dest);
			break;
		}
	}
}

cc::IVec2 AICommander::SelectTarget()
{
	return SelectTarget(ThreatScope::Units);
}

cc::IVec2 AICommander::SelectTarget(ThreatScope scope)
{
	if (scope == ThreatScope::Buildings)
	{
		Entity richest = kInvalidEntity;
		long bestWorth = 0;
		float bestDist = 0.0f;
		registry_.Each<Building>([&](Entity id, const Building &building) {
			if (building.teamID == teamID_ || building.state != BuildingState::Operational)
			{
				return;
			}
			const cc::IVec2 tile{ building.tileX, building.tileY };
			if (!Sees(tile))
			{
				return;
			}
			const UnitCost cost = BuildingCost(building.type);
			long worth = cost.iron + cost.oil;
			if (ProducerCategory(building.type).has_value())
			{
				worth *= 2;
			}
			const int dist = (tile.x - homeTile_.x) * (tile.x - homeTile_.x) +
							 (tile.y - homeTile_.y) * (tile.y - homeTile_.y);
			if (richest == kInvalidEntity || worth > bestWorth ||
				(worth == bestWorth && dist < bestDist))
			{
				richest = id;
				bestWorth = worth;
				bestDist = static_cast<float>(dist);
			}
		});
		if (richest == kInvalidEntity)
		{
			return lastSeenEnemy_;
		}
		const Building *target = registry_.Get<Building>(richest);
		lastSeenEnemy_ = { target->tileX, target->tileY };
		return lastSeenEnemy_;
	}
	Entity nearest = kInvalidEntity;
	int best = 0;
	registry_.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.teamID == teamID_ || unit.health <= 0.0f)
		{
			return;
		}
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
		if (!Sees(tile))
		{
			return;
		}
		const int dist = (tile.x - homeTile_.x) * (tile.x - homeTile_.x) +
						 (tile.y - homeTile_.y) * (tile.y - homeTile_.y);
		if (nearest == kInvalidEntity || dist < best)
		{
			nearest = id;
			best = dist;
		}
	});
	if (nearest == kInvalidEntity)
	{
		return lastSeenEnemy_;
	}
	const Unit *anchor = registry_.Get<Unit>(nearest);
	const cc::IVec2 anchorTile = cc::WorldToTile(cc::ToGlm(anchor->position));
	long sumX = 0;
	long sumY = 0;
	int count = 0;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (unit.teamID == teamID_ || unit.health <= 0.0f)
		{
			return;
		}
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
		if (!Sees(tile))
		{
			return;
		}
		const int dist = (tile.x - anchorTile.x) * (tile.x - anchorTile.x) +
						 (tile.y - anchorTile.y) * (tile.y - anchorTile.y);
		if (dist <= 36)
		{
			sumX += tile.x;
			sumY += tile.y;
			++count;
		}
	});
	if (count == 0)
	{
		return lastSeenEnemy_;
	}
	lastSeenEnemy_ = { static_cast<int>(sumX / count), static_cast<int>(sumY / count) };
	return lastSeenEnemy_;
}

static bool HarassWantsType(UnitType type)
{
	switch (type)
	{
	case UnitType::RifleInfantry:
	case UnitType::AntiArmorInfantry:
	case UnitType::PrototypeInfantry:
	case UnitType::IFV:
		return true;
	default:
		return false;
	}
}

int AICommander::Escalation() const
{
	if (matchTime_ >= 240.0f || losses_ >= 8)
	{
		return 2;
	}
	if (contact_ || matchTime_ >= 45.0f)
	{
		return 1;
	}
	return 0;
}

int AICommander::EffectiveThreshold() const
{
	if (Escalation() >= 2)
	{
		return params_.waveThreshold > 2 ? params_.waveThreshold - 1 : 2;
	}
	return params_.waveThreshold;
}

float AICommander::EffectiveCooldown() const
{
	if (Escalation() >= 2)
	{
		return params_.relaunchCooldown * 0.75f;
	}
	return params_.relaunchCooldown;
}

void AICommander::MusterTeams()
{
	for (AITeam &team : teams_)
	{
		PruneTeam(team);
	}
	std::vector<Entity> unassigned;
	registry_.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.teamID != teamID_ || unit.health <= 0.0f || unit.type == UnitType::Engineer)
		{
			return;
		}
		for (const AITeam &team : teams_)
		{
			if (std::find(team.members.begin(), team.members.end(), id) != team.members.end())
			{
				return;
			}
		}
		unassigned.push_back(id);
	});
	if (unassigned.empty())
	{
		return;
	}
	bool hasGuard = false;
	for (const AITeam &team : teams_)
	{
		if (team.role == AITeamRole::Guard && !team.members.empty())
		{
			hasGuard = true;
		}
	}
	if (!hasGuard && difficulty_ == AIDifficulty::Hard &&
		static_cast<int>(unassigned.size()) >= params_.waveThreshold + 4)
	{
		std::sort(unassigned.begin(), unassigned.end(), [&](Entity a, Entity b) {
			const Unit *ua = registry_.Get<Unit>(a);
			const Unit *ub = registry_.Get<Unit>(b);
			const float ha = ua != nullptr ? BaseStats(ua->type).health : 0.0f;
			const float hb = ub != nullptr ? BaseStats(ub->type).health : 0.0f;
			return ha > hb;
		});
		AITeam guard;
		guard.role = AITeamRole::Guard;
		guard.members.assign(unassigned.begin(), unassigned.begin() + 2);
		unassigned.erase(unassigned.begin(), unassigned.begin() + 2);
		guard.script = { { AIMissionKind::Guard, rallyTile_, 0.0f } };
		teams_.push_back(guard);
	}
	if (unassigned.size() >= 8 && difficulty_ != AIDifficulty::Easy && Escalation() >= 1)
	{
		std::vector<Entity> willing;
		for (Entity id : unassigned)
		{
			const Unit *unit = registry_.Get<Unit>(id);
			if (unit != nullptr && HarassWantsType(unit->type))
			{
				willing.push_back(id);
			}
		}
		std::sort(willing.begin(), willing.end(), [&](Entity a, Entity b) {
			const Unit *ua = registry_.Get<Unit>(a);
			const Unit *ub = registry_.Get<Unit>(b);
			const float sa = ua != nullptr ? BaseStats(ua->type).speed : 0.0f;
			const float sb = ub != nullptr ? BaseStats(ub->type).speed : 0.0f;
			return sa > sb;
		});
		const std::size_t harassCount = std::max<std::size_t>(2, unassigned.size() / 3);
		if (willing.size() >= 2)
		{
			const std::size_t take = std::min(harassCount, willing.size());
			AITeam harass;
			harass.role = AITeamRole::Harass;
			harass.members.assign(willing.begin(), willing.begin() + take);
			for (Entity id : harass.members)
			{
				unassigned.erase(std::remove(unassigned.begin(), unassigned.end(), id),
								 unassigned.end());
			}
			const cc::IVec2 target = SelectTarget();
			const int dx = target.x - homeTile_.x;
			const int dy = target.y - homeTile_.y;
			cc::IVec2 flank{ target.x - dy / 4, target.y + dx / 4 };
			flank.x = std::clamp(flank.x, 0, map_.Width() - 1);
			flank.y = std::clamp(flank.y, 0, map_.Height() - 1);
			harass.script = { { AIMissionKind::Move, flank, 30.0f },
							  { AIMissionKind::AttackMove, target, 120.0f } };
			teams_.push_back(harass);
		}
	}
	if (!unassigned.empty())
	{
		bool hasIdleMain = false;
		for (const AITeam &team : teams_)
		{
			if (team.role == AITeamRole::Main && !team.active && !team.members.empty())
			{
				hasIdleMain = true;
			}
		}
		if (!hasIdleMain)
		{
			AITeam main;
			main.role = AITeamRole::Main;
			main.members = unassigned;
			// Fight the army in sight; raze production when the field is dark.
			const ThreatScope scope =
				EnemyForceSighted() ? ThreatScope::Units : ThreatScope::Buildings;
			main.script = { { AIMissionKind::AttackMove, SelectTarget(scope), 120.0f } };
			teams_.push_back(main);
		}
		else
		{
			for (AITeam &team : teams_)
			{
				if (team.role == AITeamRole::Main && !team.active && !team.members.empty())
				{
					team.members.insert(team.members.end(), unassigned.begin(), unassigned.end());
					break;
				}
			}
		}
	}
}

void AICommander::LaunchTeams()
{
	for (AITeam &team : teams_)
	{
		if (!team.active && !team.members.empty())
		{
			team.step = 0;
			team.stepTime = 0.0f;
			team.peakSize = team.members.size();
			team.regrouping = false;
			team.active = true;
			IssueTeamStep(team);
		}
	}
	++wavesLaunched_;
	timeSinceLaunch_ = 0.0f;
}

void AICommander::UpdateTeams(float dt)
{
	for (AITeam &team : teams_)
	{
		if (!team.active)
		{
			continue;
		}
		PruneTeam(team);
		if (team.members.empty() || team.step >= team.script.size())
		{
			team.active = false;
			team.members.clear();
			continue;
		}
		if (!team.regrouping && team.peakSize >= 3 &&
			(team.role == AITeamRole::Main || team.role == AITeamRole::Harass) &&
			team.members.size() * 3 <= team.peakSize)
		{
			team.regrouping = true;
			team.script = { { AIMissionKind::Move, rallyTile_, 45.0f } };
			team.step = 0;
			team.stepTime = 0.0f;
			IssueTeamStep(team);
			continue;
		}
		team.stepTime += dt;
		const AIMissionStep &mission = team.script[team.step];
		if (mission.timeout > 0.0f && team.stepTime >= mission.timeout)
		{
			++team.step;
			team.stepTime = 0.0f;
			if (team.step < team.script.size())
			{
				IssueTeamStep(team);
			}
			else
			{
				team.active = false;
				team.members.clear();
			}
			continue;
		}
		if (TeamAllIdle(team))
		{
			if (team.step + 1 < team.script.size())
			{
				++team.step;
				team.stepTime = 0.0f;
				IssueTeamStep(team);
				continue;
			}
			if (mission.kind == AIMissionKind::Guard)
			{
				continue;
			}
			if (mission.kind == AIMissionKind::AttackMove)
			{
				ThreatScope scope = ThreatScope::Units;
				if (team.role == AITeamRole::Main && !EnemyForceSighted())
				{
					scope = ThreatScope::Buildings;
				}
				const cc::IVec2 fresh = SelectTarget(scope);
				if (!(fresh == mission.target) && team.members.size() * 2 >= team.peakSize)
				{
					team.script[team.step].target = fresh;
					team.stepTime = 0.0f;
					IssueTeamStep(team);
					continue;
				}
			}
			team.active = false;
			team.members.clear();
		}
	}
}

void AICommander::UpdateGuardPost()
{
	long sumX = 0;
	long sumY = 0;
	int count = 0;
	registry_.Each<Unit>([&](Entity, const Unit &unit) {
		if (unit.teamID == teamID_ || unit.health <= 0.0f)
		{
			return;
		}
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
		if (!Sees(tile))
		{
			return;
		}
		const int dist = (tile.x - homeTile_.x) * (tile.x - homeTile_.x) +
						 (tile.y - homeTile_.y) * (tile.y - homeTile_.y);
		if (dist <= 36)
		{
			sumX += tile.x;
			sumY += tile.y;
			++count;
		}
	});
	const bool intrusion = count > 0;
	const cc::IVec2 post = intrusion
							   ? cc::IVec2{ static_cast<int>(sumX / count),
											static_cast<int>(sumY / count) }
							   : rallyTile_;
	for (AITeam &team : teams_)
	{
		if (team.role != AITeamRole::Guard || team.members.empty() || team.script.empty())
		{
			continue;
		}
		if (!(team.script[0].target == post))
		{
			team.script[0].target = post;
			team.step = 0;
			team.stepTime = 0.0f;
			if (team.active)
			{
				IssueTeamStep(team);
			}
		}
	}
}

void AICommander::MaybeLaunchWave()
{
	MusterTeams();
	const AITeam *main = nullptr;
	for (const AITeam &team : teams_)
	{
		if (team.role == AITeamRole::Main && !team.active && !team.members.empty() &&
			static_cast<int>(team.members.size()) >= EffectiveThreshold())
		{
			main = &team;
			break;
		}
	}
	if (main == nullptr || timeSinceLaunch_ < EffectiveCooldown())
	{
		return;
	}
	LaunchTeams();
}

void AICommander::ScoutTick(float dt)
{
	scoutTimer_ -= dt;
	if (scoutTimer_ > 0.0f)
	{
		return;
	}
	const Entity scout = factory_.Spawn(UnitType::RifleInfantry, teamID_,
										cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y)));
	scoutTimer_ = params_.scoutInterval;
	if (scout == kInvalidEntity)
	{
		return;
	}
	if (Unit *unit = registry_.Get<Unit>(scout))
	{
		OrderMove(*unit, scout, cc::ToRaylib(cc::TileToWorld(enemyTile_.x, enemyTile_.y)));
	}
	scouted_ = true;
	lastSeenEnemy_ = enemyTile_;
}

void AICommander::RetreatTick()
{
	const Vector2 home = cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y));
	RetreatIfLowHP(registry_, map_, occ_, home, teamID_, kRetreatHealthFraction, false);
}
