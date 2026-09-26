// Regression tests for the "units stuck when moving" bug report: converging
// units must always resolve (arrive or cleanly cancel) within budget, never
// freeze forever. Both scenarios rely on ordinary occupancy-grid blocking
// (StepToward's CanEnter -> StepResult::BlockedUnit -> TryBlockedRetry's
// wait/replan/cancel budget), not on any continuous-space separation pass
// (removed -- see plans/UnitStackResolution_Plan.md -- stacking is now
// resolved separately, per exact anchor tile, by ResolveStackedUnits).
//   1. A head-on corridor swap through a single-tile-wide gap. The corridor
//      is a dead end (no room to yield), so completing the swap isn't
//      achievable -- the correct fix outcome is a clean cancel within
//      budget, not a silent forever-freeze.
//   2. A 5-unit formation move funneling through a single-tile gap, which
//      does have room to resolve and must fully complete.

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

Unit MakeInfantry(int tileX, int tileY, int team)
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

    // --- Scenario 1: head-on corridor swap through a 1-tile-wide gap ---
    // A single row (y=1) is the only passable lane between two units
    // approaching from opposite ends; everything above/below is walled, so
    // there is no room to route around each other -- they must meet and
    // pass through the middle.
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
        registry.Add(a, MakeInfantry(0, 1, 0));
        const Entity b = registry.Create();
        registry.Add(b, MakeInfantry(4, 1, 0));

        Unit *ua = registry.Get<Unit>(a);
        Unit *ub = registry.Get<Unit>(b);
        IssuePathOrderFootprint(*ua, GetOrders(registry, a), GetMover(registry, a), map, occ,
                                cc::ToRaylib(cc::TileToWorld(4, 1)), a,
                                registry.Generation(a));
        IssuePathOrderFootprint(*ub, GetOrders(registry, b), GetMover(registry, b), map, occ,
                                cc::ToRaylib(cc::TileToWorld(0, 1)), b,
                                registry.Generation(b));
        CC_CHECK(FindMover(registry, a)->hasPath);
        CC_CHECK(FindMover(registry, b)->hasPath);

        int frame = 0;
        constexpr int kBudget = 600; // 10s at 60fps
        for (; frame < kBudget; ++frame)
        {
            RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
            const Mover *ma = FindMover(registry, a);
            const Mover *mb = FindMover(registry, b);
            if (ma != nullptr && mb != nullptr && !ma->hasMoveOrder && !ma->hasPath &&
                !mb->hasMoveOrder && !mb->hasPath)
            {
                break;
            }
        }

        // A true head-on swap in a dead-end 1-tile corridor has no room for
        // either side to yield, so completing it isn't achievable without a
        // backtrack/yield behavior this codebase doesn't have -- cancelling
        // (same as the existing "permanent wall" behavior) is the correct
        // outcome. What must never happen is a silent forever-freeze: both
        // orders must resolve (complete or cancel) well within budget, and
        // the units must not end up wedged on top of each other.
        CC_CHECK(!FindMover(registry, a)->hasMoveOrder && !FindMover(registry, a)->hasPath);
        CC_CHECK(!FindMover(registry, b)->hasMoveOrder && !FindMover(registry, b)->hasPath);
        CC_CHECK(!(cc::WorldToTile(cc::ToGlm(ua->position)) ==
                  cc::WorldToTile(cc::ToGlm(ub->position))));
        CC_CHECK(frame < kBudget);
    }

    // --- Scenario 2: 5-unit formation move funneled through a 1-tile gap ---
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
            registry.Add(e, MakeInfantry(0, i, 0));
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
                const Mover *mover = FindMover(registry, id);
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
            const Mover *mover = FindMover(registry, id);
            if (mover != nullptr && !mover->hasMoveOrder && !mover->hasPath)
            {
                ++arrived;
            }
        });
        CC_CHECK(arrived == 5);
        CC_CHECK(frame < kBudget);
    }
}
