#include "units/AICommanderIntel.h"

#include "units/Unit.h"
#include "world/FogOfWar.h"

AICommanderIntel::AICommanderIntel(Registry &registry) : registry_(registry)
{
}

void AICommanderIntel::Reset(float scoutInterval, cc::IVec2 enemyTile, TeamId teamID)
{
	teamID_ = teamID;
	lastSeenEnemy_ = enemyTile;
	scouted_ = false;
	scoutTimer_ = scoutInterval;
	losses_ = 0;
	liveCombat_ = -1;
	contact_ = false;
}

void AICommanderIntel::SetFog(const FogOfWar *fog)
{
	fog_ = fog;
}

void AICommanderIntel::TickCombat()
{
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
}

void AICommanderIntel::RefreshIntel()
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

bool AICommanderIntel::Sees(cc::IVec2 tile) const
{
	return fog_ == nullptr || fog_->IsVisible(teamID_, tile);
}

bool AICommanderIntel::EnemyForceSighted() const
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

bool AICommanderIntel::HasScouted() const
{
	return scouted_;
}

cc::IVec2 AICommanderIntel::LastSeenEnemy() const
{
	return lastSeenEnemy_;
}

void AICommanderIntel::ReportSighting(cc::IVec2 tile)
{
	lastSeenEnemy_ = tile;
}

void AICommanderIntel::MarkScouted(cc::IVec2 enemyTile)
{
	scouted_ = true;
	lastSeenEnemy_ = enemyTile;
}

bool AICommanderIntel::SpendScoutTimer(float dt)
{
	scoutTimer_ -= dt;
	return scoutTimer_ <= 0.0f;
}

void AICommanderIntel::ResetScoutTimer(float interval)
{
	scoutTimer_ = interval;
}

int AICommanderIntel::Losses() const
{
	return losses_;
}

bool AICommanderIntel::Contact() const
{
	return contact_;
}
