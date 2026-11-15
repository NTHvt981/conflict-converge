// A stacked squad (all units on the same tile) must still fully complete a
// real formation::IssueFormationMoveFP order.

#include "test_harness.h"

#include <cstdlib>

#include "units/Extensions.h"
#include "units/Formation.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

namespace
{
Unit MakeInfantry(int tileX, int tileY)
{
	Unit unit;
	unit.type = UnitType::RifleInfantry;
	ApplyBaseStats(unit);
	unit.teamID = kPlayerTeam;
	unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	return unit;
}
} // namespace

void RunFormationStackMoveTests()
{
	TileMap map(30, 30);
	OccupancyGrid occ(30, 30);
	Registry registry;
	std::vector<Entity> squad;
	for (int i = 0; i < 4; ++i)
	{
		const Entity e = registry.Create();
		registry.Add(e, MakeInfantry(5, 5));
		squad.push_back(e);
	}

	formation::IssueFormationMoveFP(registry, squad, map, occ,
									cc::ToRaylib(cc::TileToWorld(20, 20)), false);
	for (Entity id : squad)
	{
		const Mover *mover = registry.Get<Mover>(id);
		CC_CHECK(mover != nullptr && (mover->hasMoveOrder || mover->hasPath));
	}

	// A ~15-tile diagonal march is ~21s at 64px/s; 40s is a generous budget
	// and the order must fully complete, not just make progress.
	constexpr float kDt = 1.0f / 60.0f;
	constexpr int kBudget = 2400; // 40s at 60fps
	int frame = 0;
	for (; frame < kBudget; ++frame)
	{
		RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
		bool allDone = true;
		for (Entity id : squad)
		{
			const Mover *mover = registry.Get<Mover>(id);
			allDone = allDone && mover != nullptr && !mover->hasMoveOrder && !mover->hasPath;
		}
		if (allDone)
		{
			break;
		}
	}
	CC_CHECK(frame < kBudget); // never freezes forever

	// Every unit reaches its own formation slot (2x2 grid around (20,20)),
	// and no two share a final tile.
	std::vector<cc::IVec2> finalTiles;
	for (Entity id : squad)
	{
		const Unit *u = registry.Get<Unit>(id);
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(u->position));
		CC_CHECK(std::abs(tile.x - 20) <= 1 && std::abs(tile.y - 20) <= 1);
		finalTiles.push_back(tile);
	}
	for (std::size_t i = 0; i < finalTiles.size(); ++i)
	{
		for (std::size_t j = i + 1; j < finalTiles.size(); ++j)
		{
			CC_CHECK(!(finalTiles[i] == finalTiles[j]));
		}
	}
}
