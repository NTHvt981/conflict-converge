// Unit tests for A* pathfinding and waypoint following.

#include "test_harness.h"

#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "units/UnitStats.h" // ApplyBaseStats for walkable test units

#include <cstdlib> // std::abs for adjacency checks

namespace
{

bool Adjacent(cc::IVec2 a, cc::IVec2 b)
{
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1;
}

// A valid route: endpoints match, every step is 4-adjacent, no step blocked.
bool IsValidRoute(const TileMap &map, const TilePath &path, cc::IVec2 start, cc::IVec2 goal)
{
    if (path.empty() || !(path.front() == start) || !(path.back() == goal))
    {
        return false;
    }
    for (const cc::IVec2 tile : path)
    {
        if (map.IsBlocked(tile))
        {
            return false;
        }
    }
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        if (!Adjacent(path[i - 1], path[i]))
        {
            return false;
        }
    }
    return true;
}

int WalkUntilIdle(Unit &unit, Orders &orders, Mover &mover, CombatState &combat,
                  const TileMap &map, float speed, float dt, int maxFrames)
{
    int frames = 0;
    while ((mover.hasMoveOrder || mover.hasPath) && frames < maxFrames)
    {
        UpdateUnitMovement(unit, orders, mover, combat, map, speed, dt);
        ++frames;
    }
    return frames;
}

} // namespace

void RunPathfindTests()
{
    // --- open map: optimal Manhattan route ---
    TileMap open(5, 5);
    TilePath route = FindPath(open, { 0, 0 }, { 3, 2 });
    CC_CHECK(IsValidRoute(open, route, { 0, 0 }, { 3, 2 }));
    CC_CHECK(route.size() == 6); // 3 + 2 steps -> 6 nodes

    // --- wall with a gap: route detours through it ---
    TileMap walled(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        if (y != 2)
        {
            walled.Set({ 2, y }, TerrainType::Water);
        }
    }
    TilePath detour = FindPath(walled, { 0, 0 }, { 4, 0 });
    CC_CHECK(IsValidRoute(walled, detour, { 0, 0 }, { 4, 0 }));
    bool throughGap = false;
    for (cc::IVec2 tile : detour)
    {
        throughGap = throughGap || (tile == cc::IVec2(2, 2));
    }
    CC_CHECK(throughGap);
    CC_CHECK(detour.size() > 5); // detour costs more than the open 4-step run

    // --- unreachable goal (walled in): empty path ---
    TileMap pocket(5, 5);
    pocket.Set({ 0, 1 }, TerrainType::Water);
    pocket.Set({ 1, 0 }, TerrainType::Water);
    pocket.Set({ 1, 1 }, TerrainType::Water);
    CC_CHECK(FindPath(pocket, { 0, 0 }, { 4, 4 }).empty());

    // --- degenerate inputs ---
    CC_CHECK(FindPath(open, { 1, 1 }, { 1, 1 }).size() == 1); // start == goal
    TileMap blocked(3, 3);
    blocked.Set({ 0, 0 }, TerrainType::Water);
    blocked.Set({ 2, 2 }, TerrainType::Building);
    CC_CHECK(FindPath(blocked, { 0, 0 }, { 1, 1 }).empty()); // blocked start
    CC_CHECK(FindPath(blocked, { 1, 1 }, { 2, 2 }).empty()); // blocked goal
    CC_CHECK(FindPath(blocked, { -1, 0 }, { 1, 1 }).empty()); // out of bounds

    // --- IssuePathOrder walks around the wall to a snapped arrival ---
    TileMap map(8, 6);
    for (int y = 0; y < 5; ++y)
    {
        map.Set({ 4, y }, TerrainType::Water); // vertical wall, gap at y = 5
    }
    Unit unit;
    Orders unitOrders;
    Mover unitMover;
    CombatState unitCombat;
    unit.type = UnitType::RifleInfantry;
    ApplyBaseStats(unit);
    unit.position = cc::ToRaylib(cc::TileToWorld(1, 1));
    IssuePathOrder(unit, unitOrders, unitMover, map,
                   cc::ToRaylib(cc::Vec2(6 * 64.0f + 10.0f, 1 * 64.0f + 5.0f)));
    CC_CHECK(unitMover.hasPath);
    CC_CHECK(unitMover.hasMoveOrder);
    CC_CHECK(unitMover.moveTarget.x == 6 * 64.0f); // target snapped to tile corner
    CC_CHECK(unitMover.moveTarget.y == 1 * 64.0f);

    const int frames = WalkUntilIdle(unit, unitOrders, unitMover, unitCombat, map, unit.speed,
                                     1.0f / 60.0f, 60 * 60);
    CC_CHECK(!unitMover.hasMoveOrder && !unitMover.hasPath);
    CC_CHECK(unit.state == UnitState::Idle);
    CC_CHECK(unit.position.x == 6 * 64.0f);
    CC_CHECK(unit.position.y == 1 * 64.0f);
    CC_CHECK(frames < 60 * 60); // arrived, did not time out

    // --- unreachable target falls back to a straight order ---
    Unit stuck;
    Orders stuckOrders;
    Mover stuckMover;
    stuck.position = cc::ToRaylib(cc::TileToWorld(0, 0));
    IssuePathOrder(stuck, stuckOrders, stuckMover, pocket, cc::ToRaylib(cc::TileToWorld(4, 4)));
    CC_CHECK(stuckMover.hasMoveOrder);
    CC_CHECK(!stuckMover.hasPath);
}
