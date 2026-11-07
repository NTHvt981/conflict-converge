#pragma once

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"

class FogOfWar;

class AICommanderIntel
{
public:
	explicit AICommanderIntel(Registry &registry);

	void Reset(float scoutInterval, cc::IVec2 enemyTile, TeamId teamID);
	void SetFog(const FogOfWar *fog);
	void TickCombat();
	void RefreshIntel();

	// Fog-gated visibility: omniscient when no fog is bound.
	bool Sees(cc::IVec2 tile) const;
	bool EnemyForceSighted() const;
	bool HasScouted() const;
	cc::IVec2 LastSeenEnemy() const;
	void ReportSighting(cc::IVec2 tile);
	void MarkScouted(cc::IVec2 enemyTile);
	bool SpendScoutTimer(float dt);
	void ResetScoutTimer(float interval);
	int Losses() const;
	bool Contact() const;

private:
	Registry &registry_;
	TeamId teamID_ = kEnemyTeam;
	const FogOfWar *fog_ = nullptr;
	cc::IVec2 lastSeenEnemy_{ 0, 0 };
	bool scouted_ = false;
	float scoutTimer_ = 0.0f;
	int losses_ = 0;
	int liveCombat_ = -1;
	bool contact_ = false;
};
