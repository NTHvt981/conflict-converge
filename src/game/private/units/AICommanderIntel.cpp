#include "units/AICommander.h"

#include "world/FogOfWar.h"

bool AICommander::HasScouted() const
{
	return scouted_;
}

cc::IVec2 AICommander::LastSeenEnemy() const
{
	return lastSeenEnemy_;
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
