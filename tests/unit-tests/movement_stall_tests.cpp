// Converging units always resolve within budget (arrive or cancel cleanly),
// never freeze; a dead-end 1-tile corridor swap can only cancel.

#include "test_harness.h"

#include "units/Formation.h"
#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

#include <vector>

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

void RunMovementStallTests()
{
	constexpr float kDt = 1.0f / 60.0f;

	{
		TileMap map(5, 3);
		for (int x = 0; x < 5; ++x)
		{
			map.Set({ x, 0 }, TerrainType::Water);
			map.Set({ x, 2 }, TerrainType::Water);
		}
		OccupancyGrid occ(5, 3);
		Registry registry;

		const Entity a = registry.Create();
		registry.Add(a, MakeInfantry(0, 1, kPlayerTeam));
		const Entity b = registry.Create();
		registry.Add(b, MakeInfantry(4, 1, kPlayerTeam));

		Unit *ua = registry.Get<Unit>(a);
		Unit *ub = registry.Get<Unit>(b);
		IssuePathOrderFootprint(*ua, MakeOrderContext(registry, a, map, &occ),
								cc::ToRaylib(cc::TileToWorld(4, 1)));
		IssuePathOrderFootprint(*ub, MakeOrderContext(registry, b, map, &occ),
								cc::ToRaylib(cc::TileToWorld(0, 1)));
		CC_CHECK(registry.Get<Mover>(a)->hasPath);
		CC_CHECK(registry.Get<Mover>(b)->hasPath);

		int frame = 0;
		constexpr int kBudget = 600; // 10s at 60fps
		for (; frame < kBudget; ++frame)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			const Mover *ma = registry.Get<Mover>(a);
			const Mover *mb = registry.Get<Mover>(b);
			if (ma != nullptr && mb != nullptr && !ma->hasMoveOrder && !ma->hasPath &&
				!mb->hasMoveOrder && !mb->hasPath)
			{
				break;
			}
		}

		CC_CHECK(!registry.Get<Mover>(a)->hasMoveOrder && !registry.Get<Mover>(a)->hasPath);
		CC_CHECK(!registry.Get<Mover>(b)->hasMoveOrder && !registry.Get<Mover>(b)->hasPath);
		CC_CHECK(!(cc::WorldToTile(cc::ToGlm(ua->position)) ==
				  cc::WorldToTile(cc::ToGlm(ub->position))));
		CC_CHECK(frame < kBudget);
	}

	{
		TileMap map(10, 5);
		for (int y = 0; y < 5; ++y)
		{
			if (y != 2)
			{
				map.Set({ 5, y }, TerrainType::Water);
			}
		}
		OccupancyGrid occ(10, 5);
		Registry registry;
		std::vector<Entity> squad;
		for (int i = 0; i < 5; ++i)
		{
			const Entity e = registry.Create();
			registry.Add(e, MakeInfantry(0, i, kPlayerTeam));
			squad.push_back(e);
		}

		formation::IssueFormationMoveFP(registry, squad, map, occ,
										cc::ToRaylib(cc::TileToWorld(9, 2)));

		int frame = 0;
		constexpr int kBudget = 900; // 15s at 60fps
		for (; frame < kBudget; ++frame)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			int stillMoving = 0;
			registry.Each<Unit>([&](Entity id, const Unit &u) {
				(void)u;
				const Mover *mover = registry.Get<Mover>(id);
				if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
				{
					++stillMoving;
				}
			});
			if (stillMoving == 0)
			{
				break;
			}
		}

		int arrived = 0;
		registry.Each<Unit>([&](Entity id, const Unit &u) {
			(void)u;
			const Mover *mover = registry.Get<Mover>(id);
			if (mover != nullptr && !mover->hasMoveOrder && !mover->hasPath)
			{
				++arrived;
			}
		});
		CC_CHECK(arrived == 5);
		CC_CHECK(frame < kBudget);
	}
}
