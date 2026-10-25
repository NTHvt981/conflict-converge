#include "test_harness.h"

#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

#include <cstdio>

namespace
{

Unit MakeInfantry(int tileX, int tileY, TeamId team)
{
	Unit unit;
	unit.type = UnitType::RifleInfantry;
	ApplyBaseStats(unit);
	unit.teamID = team;
	unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	return unit;
}

} // namespace

void RunYieldTests()
{
	constexpr float kDt = 1.0f / 60.0f;

	// --- Scenario 1: idle friendly blocker steps into the side pocket ---
	{
		TileMap map(7, 3);
		for (int x = 0; x < 7; ++x)
		{
			map.Set({ x, 0 }, TerrainType::Water);
			map.Set({ x, 2 }, TerrainType::Water);
		}
		map.Set({ 2, 0 }, TerrainType::Grass);
		OccupancyGrid occ(7, 3);
		Registry registry;

		const Entity moverId = registry.Create();
		registry.Add(moverId, MakeInfantry(0, 1, kPlayerTeam));
		const Entity blockerId = registry.Create();
		registry.Add(blockerId, MakeInfantry(2, 1, kPlayerTeam));

		Unit *moverUnit = registry.Get<Unit>(moverId);
		Unit *blockerUnit = registry.Get<Unit>(blockerId);
		IssuePathOrderFootprint(*moverUnit, MakeOrderContext(registry, moverId, map, &occ),
								cc::ToRaylib(cc::TileToWorld(6, 1)));

		int frames = 0;
		constexpr int kBudget = 900;
		for (; frames < kBudget; ++frames)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			const Mover *mover = FindMover(registry, moverId);
			if (mover != nullptr && !mover->hasMoveOrder && !mover->hasPath &&
				cc::WorldToTile(cc::ToGlm(moverUnit->position)) == cc::IVec2(6, 1))
			{
				break;
			}
		}
		std::printf("yield: corridor arrival %.2fs\n", frames * kDt);

		CC_CHECK(cc::WorldToTile(cc::ToGlm(moverUnit->position)) == cc::IVec2(6, 1));
		CC_CHECK(!FindMover(registry, moverId)->hasMoveOrder);
		CC_CHECK(cc::WorldToTile(cc::ToGlm(blockerUnit->position)) == cc::IVec2(2, 0));
		CC_CHECK(blockerUnit->state == UnitState::Idle);
	}

	// --- Scenario 2: no side pocket, the blocker declines and the mover
	// cancels cleanly instead of freezing ---
	{
		TileMap map(7, 3);
		for (int x = 0; x < 7; ++x)
		{
			map.Set({ x, 0 }, TerrainType::Water);
			map.Set({ x, 2 }, TerrainType::Water);
		}
		OccupancyGrid occ(7, 3);
		Registry registry;

		const Entity moverId = registry.Create();
		registry.Add(moverId, MakeInfantry(0, 1, kPlayerTeam));
		const Entity blockerId = registry.Create();
		registry.Add(blockerId, MakeInfantry(2, 1, kPlayerTeam));

		Unit *moverUnit = registry.Get<Unit>(moverId);
		IssuePathOrderFootprint(*moverUnit, MakeOrderContext(registry, moverId, map, &occ),
								cc::ToRaylib(cc::TileToWorld(6, 1)));

		int frames = 0;
		constexpr int kBudget = 600;
		for (; frames < kBudget; ++frames)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			const Mover *mover = FindMover(registry, moverId);
			if (mover != nullptr && !mover->hasMoveOrder && !mover->hasPath)
			{
				break;
			}
		}

		CC_CHECK(!FindMover(registry, moverId)->hasMoveOrder);
		CC_CHECK(cc::WorldToTile(cc::ToGlm(moverUnit->position)) != cc::IVec2(6, 1));
	}

	// --- Scenario 3: an enemy blocker never steps aside ---
	{
		TileMap map(7, 3);
		for (int x = 0; x < 7; ++x)
		{
			map.Set({ x, 0 }, TerrainType::Water);
			map.Set({ x, 2 }, TerrainType::Water);
		}
		OccupancyGrid occ(7, 3);
		Registry registry;

		const Entity moverId = registry.Create();
		registry.Add(moverId, MakeInfantry(0, 1, kPlayerTeam));
		const Entity blockerId = registry.Create();
		registry.Add(blockerId, MakeInfantry(2, 1, kEnemyTeam));

		Unit *moverUnit = registry.Get<Unit>(moverId);
		Unit *blockerUnit = registry.Get<Unit>(blockerId);
		IssuePathOrderFootprint(*moverUnit, MakeOrderContext(registry, moverId, map, &occ),
								cc::ToRaylib(cc::TileToWorld(6, 1)));

		int frames = 0;
		constexpr int kBudget = 600;
		for (; frames < kBudget; ++frames)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			const Mover *mover = FindMover(registry, moverId);
			if (mover != nullptr && !mover->hasMoveOrder && !mover->hasPath)
			{
				break;
			}
		}

		CC_CHECK(!FindMover(registry, moverId)->hasMoveOrder);
		CC_CHECK(cc::WorldToTile(cc::ToGlm(blockerUnit->position)) == cc::IVec2(2, 1));
	}

	// --- Scenario 4: an idle unit beside the path is never asked to move ---
	{
		TileMap map(7, 3);
		for (int x = 0; x < 7; ++x)
		{
			map.Set({ x, 0 }, TerrainType::Water);
			map.Set({ x, 2 }, TerrainType::Water);
		}
		map.Set({ 2, 0 }, TerrainType::Grass);
		OccupancyGrid occ(7, 3);
		Registry registry;

		const Entity moverId = registry.Create();
		registry.Add(moverId, MakeInfantry(0, 1, kPlayerTeam));
		const Entity bystanderId = registry.Create();
		registry.Add(bystanderId, MakeInfantry(2, 0, kPlayerTeam));

		Unit *moverUnit = registry.Get<Unit>(moverId);
		Unit *bystanderUnit = registry.Get<Unit>(bystanderId);
		IssuePathOrderFootprint(*moverUnit, MakeOrderContext(registry, moverId, map, &occ),
								cc::ToRaylib(cc::TileToWorld(6, 1)));

		int frames = 0;
		constexpr int kBudget = 600;
		for (; frames < kBudget; ++frames)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			if (cc::WorldToTile(cc::ToGlm(moverUnit->position)) == cc::IVec2(6, 1))
			{
				break;
			}
		}

		CC_CHECK(cc::WorldToTile(cc::ToGlm(moverUnit->position)) == cc::IVec2(6, 1));
		CC_CHECK(cc::WorldToTile(cc::ToGlm(bystanderUnit->position)) == cc::IVec2(2, 0));
		CC_CHECK(!FindMover(registry, bystanderId)->hasMoveOrder);
	}
}
