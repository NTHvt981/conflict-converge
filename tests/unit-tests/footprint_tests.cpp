// Unit tests for Phase 4: multi-tile footprints, OccupancyGrid, and
// footprint-aware pathfinding (8-dir A*, corner-cutting prevention).

#include "test_harness.h"

#include "Pathfinder.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{

bool IsValidFootprintRoute(const TileMap &map, const OccupancyGrid &occ,
                            const TilePath &path, cc::IVec2 start, cc::IVec2 goal,
                            int fpW, int fpH, Entity self, std::uint32_t gen)
{
    if (path.empty() || !(path.front() == start) || !(path.back() == goal))
    {
        return false;
    }
    for (const cc::IVec2 tile : path)
    {
        if (!occ.CanEnter(map, tile, fpW, fpH, self, gen))
        {
            return false;
        }
    }
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        const cc::IVec2 prev = path[i - 1];
        const cc::IVec2 cur = path[i];
        const int dx = cur.x - prev.x;
        const int dy = cur.y - prev.y;
        // 8-dir: each step moves at most 1 tile in each axis
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1)
        {
            return false;
        }
    }
    return true;
}

int WalkUntilIdle(Unit &unit, const TileMap &map, float speed, float dt, int maxFrames)
{
    int frames = 0;
    while ((unit.hasMoveOrder || unit.hasPath) && frames < maxFrames)
    {
        UpdateUnitMovement(unit, map, speed, dt);
        ++frames;
    }
    return frames;
}

} // namespace

void RunFootprintTests()
{
    // --- OccupancyGrid: basic set/get ---
    {
        OccupancyGrid occ(8, 8);
        CC_CHECK(occ.Width() == 8);
        CC_CHECK(occ.Height() == 8);
        CC_CHECK(!occ.InBounds({ -1, 0 }));
        CC_CHECK(occ.InBounds({ 0, 0 }));
        CC_CHECK(occ.InBounds({ 7, 7 }));

        // Unit occupancy
        CC_CHECK(occ.GetUnit({ 3, 4 }).entity == kOccEmpty);
        occ.SetUnit({ 3, 4 }, 42, 5);
        CC_CHECK(occ.GetUnit({ 3, 4 }).entity == 42);
        CC_CHECK(occ.GetUnit({ 3, 4 }).generation == 5);

        // Building occupancy
        CC_CHECK(occ.GetBuilding({ 1, 1 }) == kOccEmpty);
        occ.SetBuilding({ 1, 1 }, 99);
        CC_CHECK(occ.GetBuilding({ 1, 1 }) == 99);
    }

    // --- ReserveFootprint / ReleaseFootprint ---
    {
        OccupancyGrid occ(8, 8);
        occ.ReserveFootprint({ 2, 2 }, 2, 2, 10, 1);
        // All 4 tiles of a 2x2 footprint reserved
        CC_CHECK(occ.GetUnit({ 2, 2 }).entity == 10);
        CC_CHECK(occ.GetUnit({ 3, 2 }).entity == 10);
        CC_CHECK(occ.GetUnit({ 2, 3 }).entity == 10);
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == 10);
        // Adjacent tile is free
        CC_CHECK(occ.GetUnit({ 4, 2 }).entity == kOccEmpty);

        occ.ReleaseFootprint({ 2, 2 }, 2, 2);
        CC_CHECK(occ.GetUnit({ 2, 2 }).entity == kOccEmpty);
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == kOccEmpty);
    }

    // --- CanEnter: terrain blocking ---
    {
        TileMap map(8, 8);
        map.Set({ 3, 3 }, TerrainType::Water);
        OccupancyGrid occ(8, 8);
        // 1x1 unit can enter (2,2) but not (3,3) — water
        CC_CHECK(occ.CanEnter(map, { 2, 2 }, 1, 1, 0, 1));
        CC_CHECK(!occ.CanEnter(map, { 3, 3 }, 1, 1, 0, 1));
        // 2x2 unit can enter (1,1) — all grass
        CC_CHECK(occ.CanEnter(map, { 1, 1 }, 2, 2, 0, 1));
        // 2x2 unit cannot enter (2,2) — extends into (3,3) which is water
        CC_CHECK(!occ.CanEnter(map, { 2, 2 }, 2, 2, 0, 1));
    }

    // --- CanEnter: unit occupancy blocking ---
    {
        TileMap map(8, 8);
        OccupancyGrid occ(8, 8);
        occ.ReserveFootprint({ 3, 3 }, 2, 2, 10, 1);
        // Same entity+gen: allowed (self-overlap)
        CC_CHECK(occ.CanEnter(map, { 3, 3 }, 2, 2, 10, 1));
        // Different entity: blocked
        CC_CHECK(!occ.CanEnter(map, { 3, 3 }, 2, 2, 20, 1));
        // Partial overlap: 1x1 unit at (4,4) is inside the 2x2 footprint
        CC_CHECK(!occ.CanEnter(map, { 4, 4 }, 1, 1, 20, 1));
        // Adjacent non-overlapping: OK
        CC_CHECK(occ.CanEnter(map, { 5, 3 }, 2, 2, 20, 1));
    }

    // --- CanEnter: stale generation rejected ---
    {
        TileMap map(8, 8);
        OccupancyGrid occ(8, 8);
        occ.ReserveFootprint({ 0, 0 }, 1, 1, 10, 1);
        // Same entity, old generation: blocked
        CC_CHECK(!occ.CanEnter(map, { 0, 0 }, 1, 1, 10, 0));
        // Same entity, correct generation: allowed
        CC_CHECK(occ.CanEnter(map, { 0, 0 }, 1, 1, 10, 1));
    }

    // --- CanEnter: out-of-bounds ---
    {
        TileMap map(8, 8);
        OccupancyGrid occ(8, 8);
        CC_CHECK(!occ.CanEnter(map, { -1, 0 }, 1, 1, 0, 1));
        CC_CHECK(!occ.CanEnter(map, { 7, 7 }, 2, 2, 0, 1)); // extends past edge
    }

    // --- ApplyBaseStats sets footprints ---
    {
        Unit inf;
        inf.type = UnitType::Infantry;
        ApplyBaseStats(inf);
        CC_CHECK(inf.footprintWidth == 1);
        CC_CHECK(inf.footprintHeight == 1);

        Unit tank;
        tank.type = UnitType::HeavyTank;
        ApplyBaseStats(tank);
        CC_CHECK(tank.footprintWidth == 2);
        CC_CHECK(tank.footprintHeight == 2);

        Unit arty;
        arty.type = UnitType::Artillery;
        ApplyBaseStats(arty);
        CC_CHECK(arty.footprintWidth == 2);
        CC_CHECK(arty.footprintHeight == 2);

        Unit ifv;
        ifv.type = UnitType::IFV;
        ApplyBaseStats(ifv);
        CC_CHECK(ifv.footprintWidth == 2);
        CC_CHECK(ifv.footprintHeight == 2);
    }

    // --- 1x1 pathfinding still works (backward compat) ---
    {
        TileMap map(8, 8);
        OccupancyGrid occ(8, 8);
        TilePath route = FindPathFootprint(map, occ, { 0, 0 }, { 3, 3 }, 1, 1, 0, 1);
        CC_CHECK(!route.empty());
        CC_CHECK(route.front() == cc::IVec2(0, 0));
        CC_CHECK(route.back() == cc::IVec2(3, 3));
    }

    // --- 2x2 pathfinding: full wall with no gap forces detour ---
    {
        TileMap map(8, 8);
        for (int y = 0; y < 8; ++y)
        {
            map.Set({ 4, y }, TerrainType::Water); // full vertical wall
        }
        OccupancyGrid occ(8, 8);
        // 2x2 unit from (1,3) to (6,3): wall blocks direct path.
        // Must go around via map edges (top or bottom).
        TilePath route = FindPathFootprint(map, occ, { 1, 3 }, { 6, 3 }, 2, 2, 0, 1);
        // On an 8x8 map with a full wall, the 2x2 can't cross — goal unreachable
        CC_CHECK(route.empty());
    }

    // --- 2x2 pathfinding: wide gap lets 2x2 through ---
    {
        TileMap map(8, 8);
        for (int y = 0; y < 8; ++y)
        {
            map.Set({ 4, y }, TerrainType::Water);
        }
        map.Set({ 4, 3 }, TerrainType::Grass); // gap row 3
        map.Set({ 4, 4 }, TerrainType::Grass); // gap row 4 (2 tiles wide = fits 2x2)
        OccupancyGrid occ(8, 8);
        TilePath route = FindPathFootprint(map, occ, { 1, 3 }, { 6, 3 }, 2, 2, 0, 1);
        CC_CHECK(!route.empty());
        CC_CHECK(IsValidFootprintRoute(map, occ, route, { 1, 3 }, { 6, 3 }, 2, 2, 0, 1));
    }

    // --- 2x2 vs 1x1: 1-tile gap blocks 2x2 but admits 1x1 ---
    {
        TileMap map(10, 6);
        // Wall at x=5, gap only at y=2
        for (int y = 0; y < 6; ++y)
        {
            if (y != 2)
            {
                map.Set({ 5, y }, TerrainType::Water);
            }
        }
        OccupancyGrid occ(10, 6);
        // 1x1 can fit through the gap at y=2
        TilePath route1x1 = FindPathFootprint(map, occ, { 0, 2 }, { 7, 2 }, 1, 1, 0, 1);
        CC_CHECK(!route1x1.empty());

        // 2x2 cannot fit through the 1-tile gap; no path exists on this map
        TilePath route2x2 = FindPathFootprint(map, occ, { 0, 2 }, { 7, 2 }, 2, 2, 0, 1);
        CC_CHECK(route2x2.empty());
    }

    // --- Corner-cutting prevention for 2x2 ---
    {
        // Two parallel walls creating a corridor that 2x2 can pass through
        TileMap map(8, 6);
        map.Set({ 3, 0 }, TerrainType::Water);
        map.Set({ 3, 1 }, TerrainType::Water);
        map.Set({ 4, 4 }, TerrainType::Water);
        map.Set({ 4, 5 }, TerrainType::Water);
        OccupancyGrid occ(8, 6);
        // 2x2 from (1,2) to (6,2): corridor is 3 tiles wide, fits 2x2
        TilePath route = FindPathFootprint(map, occ, { 1, 2 }, { 6, 2 }, 2, 2, 0, 1);
        CC_CHECK(!route.empty());
        CC_CHECK(IsValidFootprintRoute(map, occ, route, { 1, 2 }, { 6, 2 }, 2, 2, 0, 1));
    }

    // --- 2x2 unit movement via UpdateUnitMovement with occupancy ---
    {
        TileMap map(8, 8);
        OccupancyGrid occ(8, 8);
        Unit unit;
        unit.type = UnitType::HeavyTank;
        ApplyBaseStats(unit);
        unit.position = cc::ToRaylib(cc::TileToWorld(0, 0));
        occ.ReserveFootprint({ 0, 0 }, 2, 2, 1, 1);

        IssueMoveOrder(unit, cc::ToRaylib(cc::TileToWorld(3, 0)));
        const int frames = WalkUntilIdle(unit, map, unit.speed, 1.0f / 60.0f, 300);
        CC_CHECK(!unit.hasMoveOrder);
        CC_CHECK(unit.state == UnitState::Idle);
        CC_CHECK(frames < 300);
    }
}
