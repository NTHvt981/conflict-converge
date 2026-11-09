#include "units/AICommanderTeams.h"

#include <algorithm>

#include "units/AICommanderIntel.h"
#include "economy/Building.h"
#include "units/Extensions.h"
#include "units/Formation.h"
#include "units/UnitStats.h"
#include "world/TileMap.h"

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

namespace
{
enum class BaseStat
{
	Health,
	Speed
};

struct ByBaseStat
{
	const Registry &registry;
	BaseStat stat;

	bool operator()(Entity lhs, Entity rhs) const;
};

bool ByBaseStat::operator()(Entity lhs, Entity rhs) const
{
	const auto value = [this](Entity id) {
		const Unit *unit = registry.Get<Unit>(id);
		if (unit == nullptr)
		{
			return 0.0f;
		}
		const UnitStats &base = BaseStats(unit->type);
		return stat == BaseStat::Health ? base.health : base.speedTilesPerSec;
	};
	return value(lhs) > value(rhs);
}
} // namespace

AICommanderTeams::AICommanderTeams(Registry &registry, TileMap &map, AICommanderIntel &intel,
								   AICommanderEconomy &economy)
	: registry_(registry), map_(map), intel_(intel), economy_(economy)
{
}

void AICommanderTeams::Reset(TeamId teamID, const AIDifficultyParams &params, cc::IVec2 homeTile,
							 cc::IVec2 enemyTile)
{
	teamID_ = teamID;
	homeTile_ = homeTile;
	enemyTile_ = enemyTile;
	rallyTile_ = homeTile + cc::IVec2(0, 3);
	teams_.clear();
	timeSinceLaunch_ = params.relaunchCooldown;
	matchTime_ = 0.0f;
	wavesLaunched_ = 0;
}

void AICommanderTeams::SetOccupancy(OccupancyGrid *occ)
{
	occ_ = occ;
}

int AICommanderTeams::WavesLaunched() const
{
	return wavesLaunched_;
}

void AICommanderTeams::PruneTeam(AITeam &team)
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

bool AICommanderTeams::TeamAllIdle(const AITeam &team)
{
	for (Entity id : team.members)
	{
		const Mover *mover = registry_.Get<Mover>(id);
		if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
		{
			return false;
		}
	}
	return true;
}

void AICommanderTeams::IssueTeamStep(AITeam &team)
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
			economy_.OrderMove(*unit, team.members[i], dest);
			break;
		case AIMissionKind::Guard:
			economy_.OrderMove(*unit, team.members[i], dest);
			if (CombatState *combat = registry_.Get<CombatState>(team.members[i]))
			{
				SetStance(*unit, GetOrders(registry_, team.members[i]), *combat, Stance::Guard);
			}
			break;
		case AIMissionKind::AttackMove:
		default:
			economy_.OrderAttackMove(*unit, team.members[i], dest);
			break;
		}
	}
}

cc::IVec2 AICommanderTeams::SelectTarget()
{
	return SelectTarget(ThreatScope::Units);
}

cc::IVec2 AICommanderTeams::SelectTarget(ThreatScope scope)
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
			if (!intel_.Sees(tile))
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
			return intel_.LastSeenEnemy();
		}
		const Building *target = registry_.Get<Building>(richest);
		const cc::IVec2 sighted{ target->tileX, target->tileY };
		intel_.ReportSighting(sighted);
		return sighted;
	}
	Entity nearest = kInvalidEntity;
	int best = 0;
	registry_.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.teamID == teamID_ || unit.health <= 0.0f)
		{
			return;
		}
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit.position));
		if (!intel_.Sees(tile))
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
		return intel_.LastSeenEnemy();
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
		if (!intel_.Sees(tile))
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
		return intel_.LastSeenEnemy();
	}
	const cc::IVec2 sighted{ static_cast<int>(sumX / count), static_cast<int>(sumY / count) };
	intel_.ReportSighting(sighted);
	return sighted;
}

int AICommanderTeams::Escalation() const
{
	if (matchTime_ >= 240.0f || intel_.Losses() >= 8)
	{
		return 2;
	}
	if (intel_.Contact() || matchTime_ >= 45.0f)
	{
		return 1;
	}
	return 0;
}

int AICommanderTeams::EffectiveThreshold() const
{
	const int threshold = economy_.Params().waveThreshold;
	if (Escalation() >= 2)
	{
		return threshold > 2 ? threshold - 1 : 2;
	}
	return threshold;
}

float AICommanderTeams::EffectiveCooldown() const
{
	const float cooldown = economy_.Params().relaunchCooldown;
	if (Escalation() >= 2)
	{
		return cooldown * 0.75f;
	}
	return cooldown;
}

void AICommanderTeams::MusterTeams()
{
	for (AITeam &team : teams_)
	{
		PruneTeam(team);
	}
	std::vector<Entity> unassigned = GatherUnassignedUnits();
	if (unassigned.empty())
	{
		return;
	}
	EnsureGuardTeam(unassigned);
	EnsureHarassTeam(unassigned);
	AssignRemainingToMain(unassigned);
}

std::vector<Entity> AICommanderTeams::GatherUnassignedUnits()
{
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
	return unassigned;
}

void AICommanderTeams::EnsureGuardTeam(std::vector<Entity> &unassigned)
{
	bool hasGuard = false;
	for (const AITeam &team : teams_)
	{
		if (team.role == AITeamRole::Guard && !team.members.empty())
		{
			hasGuard = true;
		}
	}
	if (!hasGuard && economy_.Difficulty() == AIDifficulty::Hard &&
		static_cast<int>(unassigned.size()) >= economy_.Params().waveThreshold + 4)
	{
		std::sort(unassigned.begin(), unassigned.end(), ByBaseStat{ registry_, BaseStat::Health });
		AITeam guard;
		guard.role = AITeamRole::Guard;
		guard.members.assign(unassigned.begin(), unassigned.begin() + 2);
		unassigned.erase(unassigned.begin(), unassigned.begin() + 2);
		guard.script = { { AIMissionKind::Guard, rallyTile_, 0.0f } };
		teams_.push_back(guard);
	}
}

void AICommanderTeams::EnsureHarassTeam(std::vector<Entity> &unassigned)
{
	if (unassigned.size() >= 8 && economy_.Difficulty() != AIDifficulty::Easy &&
		Escalation() >= 1)
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
		std::sort(willing.begin(), willing.end(), ByBaseStat{ registry_, BaseStat::Speed });
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
}

void AICommanderTeams::AssignRemainingToMain(std::vector<Entity> &unassigned)
{
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
			const ThreatScope scope =
				intel_.EnemyForceSighted() ? ThreatScope::Units : ThreatScope::Buildings;
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

void AICommanderTeams::LaunchTeams()
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

void AICommanderTeams::UpdateTeams(float dt)
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
				if (team.role == AITeamRole::Main && !intel_.EnemyForceSighted())
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

void AICommanderTeams::UpdateGuardPost()
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
		if (!intel_.Sees(tile))
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

void AICommanderTeams::MaybeLaunchWave()
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

void AICommanderTeams::ScoutTick(float dt)
{
	if (!intel_.SpendScoutTimer(dt))
	{
		return;
	}
	const Entity scout = economy_.Factory().Spawn(UnitType::RifleInfantry, teamID_,
												  cc::ToRaylib(cc::TileToWorld(homeTile_.x,
																			  homeTile_.y)));
	intel_.ResetScoutTimer(economy_.Params().scoutInterval);
	if (scout == kInvalidEntity)
	{
		return;
	}
	if (Unit *unit = registry_.Get<Unit>(scout))
	{
		economy_.OrderMove(*unit, scout,
						   cc::ToRaylib(cc::TileToWorld(enemyTile_.x, enemyTile_.y)));
	}
	intel_.MarkScouted(enemyTile_);
}

void AICommanderTeams::RetreatTick()
{
	const Vector2 home = cc::ToRaylib(cc::TileToWorld(homeTile_.x, homeTile_.y));
	RetreatIfLowHP(registry_, map_, occ_, home, teamID_, kRetreatHealthFraction, false);
}

void AICommanderTeams::Update(float dt)
{
	timeSinceLaunch_ += dt;
	matchTime_ += dt;
	MaybeLaunchWave();
	UpdateTeams(dt);
	UpdateGuardPost();
	ScoutTick(dt);
	if (economy_.Params().retreats)
	{
		RetreatTick();
	}
}
