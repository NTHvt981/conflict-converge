// Coverage gap closed after a live-game report of a squad that looked stuck
// after a formation move order: a stacked squad (all units on the exact same
// tile, as a freshly production-queued squad sits before ResolveStackedUnits
// has a chance to spread them -- see plans/UnitStackResolution_Plan.md) must
// still fully complete a real formation::IssueFormationMoveFP order, the same
// dispatch PlayingInput.cpp uses for a plain multi-unit right-click move.

#include "test_harness.h"

#include <cstdlib>

#include "Formation.h"
#include "Pathfinder.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{
Unit MakeInfantry(int tileX, int tileY)
{
    Unit unit;
    unit.type = UnitType::Infantry;
    ApplyBaseStats(unit);
    unit.teamID = 0;
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
        const Unit *u = registry.Get<Unit>(id);
        CC_CHECK(u->hasMoveOrder || u->hasPath);
    }

    // Infantry crosses one diagonal tile (~90.5px at 64px/s) in ~1.4s; the
    // full ~15-tile diagonal march is ~21s. 40s is a generous budget --
    // this must fully complete, not just make partial progress.
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kBudget = 2400; // 40s at 60fps
    int frame = 0;
    for (; frame < kBudget; ++frame)
    {
        RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
        bool allDone = true;
        for (Entity id : squad)
        {
            const Unit *u = registry.Get<Unit>(id);
            allDone = allDone && !u->hasMoveOrder && !u->hasPath;
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
