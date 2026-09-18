// Unit tests for multi-tile footprints, OccupancyGrid, and
// footprint-aware pathfinding (8-dir A*, corner-cutting prevention).

#include "test_harness.h"

#include "Formation.h"
#include "Pathfinder.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

#include <set>
#include <tuple>

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

    // --- NearestEnterableTile: goal sanitization ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        // Free goal returns unchanged.
        CC_CHECK(NearestEnterableTile(map, occ, { 5, 5 }, 1, 1, 7, 1) == cc::IVec2(5, 5));
        // Occupied goal (other entity) shifts to an enterable neighbor.
        occ.ReserveFootprint({ 5, 5 }, 1, 1, 9, 1);
        const cc::IVec2 near = NearestEnterableTile(map, occ, { 5, 5 }, 1, 1, 7, 1);
        CC_CHECK(!(near == cc::IVec2(5, 5)));
        CC_CHECK(occ.CanEnter(map, near, 1, 1, 7, 1));
        // Self-occupied goal stays (self-exclusion).
        occ.ReserveFootprint({ 2, 2 }, 1, 1, 7, 1);
        CC_CHECK(NearestEnterableTile(map, occ, { 2, 2 }, 1, 1, 7, 1) == cc::IVec2(2, 2));
        // Nothing enterable nearby returns the request unchanged.
        for (int y = 0; y < 10; ++y)
        {
            for (int x = 0; x < 10; ++x)
            {
                occ.ReserveFootprint({ x, y }, 1, 1, 9, 1);
            }
        }
        CC_CHECK(NearestEnterableTile(map, occ, { 5, 5 }, 1, 1, 7, 1) == cc::IVec2(5, 5));
    }

    // --- IssuePathOrderFootprint: occupied goal stops beside the blocker ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity mover = registry.Create();
        Unit unit;
        unit.footprintWidth = 1;
        unit.footprintHeight = 1;
        unit.position = cc::ToRaylib(cc::TileToWorld(0, 0));
        registry.Add(mover, unit);
        const Entity blocker = registry.Create();
        occ.ReserveFootprint({ 5, 5 }, 1, 1, blocker, registry.Generation(blocker));

        Unit *m = registry.Get<Unit>(mover);
        IssuePathOrderFootprint(*m, map, occ, cc::ToRaylib(cc::TileToWorld(5, 5)), mover,
                                registry.Generation(mover));
        CC_CHECK(m->hasPath); // routed, not straight-fallback-cancelled
        const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(m->moveTarget));
        CC_CHECK(!(dest == cc::IVec2(5, 5)));
        CC_CHECK(occ.CanEnter(map, dest, 1, 1, mover, registry.Generation(mover)));
    }

    // --- IssueFormationMoveFP: slots on blockers sanitize, no shared tile ---
    {
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        Registry registry;
        std::vector<Entity> squad;
        for (int i = 0; i < 2; ++i)
        {
            Unit unit;
            unit.footprintWidth = 1;
            unit.footprintHeight = 1;
            unit.position = cc::ToRaylib(cc::TileToWorld(i, 0));
            squad.push_back(registry.Create());
            registry.Add(squad.back(), unit);
        }
        const Entity blocker = registry.Create();
        occ.ReserveFootprint({ 10, 10 }, 1, 1, blocker, registry.Generation(blocker));

        formation::IssueFormationMoveFP(registry, squad, map, occ,
                                        cc::ToRaylib(cc::TileToWorld(10, 10)));
        std::set<std::pair<int, int>> dests;
        int orderedCount = 0;
        registry.Each<Unit>([&](Entity id, const Unit &u) {
            if (id == blocker)
            {
                return;
            }
            if (u.hasMoveOrder || u.hasPath)
            {
                ++orderedCount;
            }
            const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(u.moveTarget));
            CC_CHECK(!(dest == cc::IVec2(10, 10))); // nobody drives into the blocker
            dests.insert({ dest.x, dest.y });
        });
        CC_CHECK(orderedCount == 2);
        CC_CHECK(dests.size() == 2);
    }

    // --- Owned release/reserve: nobody wipes or steals another anchor ---
    {
        OccupancyGrid occ(8, 8);
        occ.ReserveFootprint({ 3, 3 }, 1, 1, 10, 1);
        // Foreign release leaves the cell alone; owned release clears it.
        occ.ReleaseFootprintOwned({ 3, 3 }, 1, 1, 99, 1);
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == 10);
        occ.ReleaseFootprintOwned({ 3, 3 }, 1, 1, 10, 2); // stale generation
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == 10);
        occ.ReleaseFootprintOwned({ 3, 3 }, 1, 1, 10, 1);
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == kOccEmpty);
        // Owned reserve stamps free cells but never clobbers another anchor.
        occ.ReserveFootprint({ 4, 4 }, 1, 1, 10, 1);
        CC_CHECK(occ.ReserveFootprintOwned({ 3, 3 }, 2, 2, 20, 1) == 3);
        CC_CHECK(occ.GetUnit({ 4, 4 }).entity == 10); // untouched
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == 20);
        // Fully overlapped reserve takes nothing.
        CC_CHECK(occ.ReserveFootprintOwned({ 4, 4 }, 1, 1, 30, 1) == 0);
        CC_CHECK(occ.GetUnit({ 4, 4 }).entity == 10);
        // Pre-pass order (release own, then reserve) with a foreign squatter:
        // own cells re-stamp, the foreign cell stays foreign, count is partial.
        occ.ReleaseFootprintOwned({ 3, 3 }, 2, 2, 20, 1);
        CC_CHECK(occ.GetUnit({ 4, 4 }).entity == 10);
        CC_CHECK(occ.ReserveFootprintOwned({ 3, 3 }, 2, 2, 20, 1) == 3);
        CC_CHECK(occ.GetUnit({ 3, 3 }).entity == 20);
        CC_CHECK(occ.GetUnit({ 4, 4 }).entity == 10);
    }

    // --- Blocked retry: transient blocker waits, replans, arrives ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity mover = registry.Create();
        Unit unit;
        unit.footprintWidth = 1;
        unit.footprintHeight = 1;
        unit.speed = 120.0f;
        unit.position = cc::ToRaylib(cc::TileToWorld(0, 0));
        registry.Add(mover, unit);
        const Entity blocker = registry.Create();

        Unit *m = registry.Get<Unit>(mover);
        IssuePathOrderFootprint(*m, map, occ, cc::ToRaylib(cc::TileToWorld(4, 0)), mover,
                                registry.Generation(mover));
        CC_CHECK(m->hasPath);
        // Drop a blocker onto the route mid-walk (old code cancels here).
        occ.ReserveFootprint({ 2, 0 }, 1, 1, blocker, registry.Generation(blocker));
        int frames = 0;
        while ((m->hasMoveOrder || m->hasPath) && frames < 1200)
        {
            UpdateUnitMovement(*m, map, m->speed, 1.0f / 60.0f, &occ, mover,
                               registry.Generation(mover));
            ++frames;
        }
        CC_CHECK(!m->hasMoveOrder && !m->hasPath); // arrived, not stuck
        CC_CHECK(cc::WorldToTile(cc::ToGlm(m->position)) == cc::IVec2(4, 0));
        CC_CHECK(frames > 60); // survived the block (instant-cancel dies ~32)
        CC_CHECK(frames < 1200);
    }

    // --- Blocked retry: permanent wall exhausts the budget and cancels ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity mover = registry.Create();
        Unit unit;
        unit.footprintWidth = 1;
        unit.footprintHeight = 1;
        unit.speed = 120.0f;
        unit.position = cc::ToRaylib(cc::TileToWorld(0, 0));
        registry.Add(mover, unit);
        const Entity wall = registry.Create();
        const std::uint32_t wallGen = registry.Generation(wall);
        occ.ReserveFootprint({ 1, 0 }, 1, 1, wall, wallGen);
        occ.ReserveFootprint({ 0, 1 }, 1, 1, wall, wallGen);
        occ.ReserveFootprint({ 1, 1 }, 1, 1, wall, wallGen);
        occ.ReserveFootprint({ 2, 1 }, 1, 1, wall, wallGen);

        Unit *m = registry.Get<Unit>(mover);
        IssueMoveOrder(*m, cc::ToRaylib(cc::TileToWorld(2, 0))); // straight at the wall
        int frames = 0;
        while ((m->hasMoveOrder || m->hasPath) && frames < 600)
        {
            UpdateUnitMovement(*m, map, m->speed, 1.0f / 60.0f, &occ, mover,
                               registry.Generation(mover));
            ++frames;
        }
        CC_CHECK(!m->hasMoveOrder && !m->hasPath); // gave up, order gone
        CC_CHECK(m->state == UnitState::Idle);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(m->position)) == cc::IVec2(0, 0));
        CC_CHECK(frames > 90); // waited through retries (3 x 0.5s), not instant
        CC_CHECK(frames < 600);
    }

    // --- Blocked start still paths: units live on building tiles ---
    // Spawns, rally points, and harvesters sit inside/on footprints; the
    // search must route out of them (StepToward's step-out allowance covers
    // the first step) instead of stranding the unit on straight fallback.
    {
        TileMap map(10, 10);
        map.Set({ 0, 0 }, TerrainType::Building);
        OccupancyGrid occ(10, 10);
        // Terrain-blocked anchor routes out.
        const TilePath route = FindPathFootprint(map, occ, { 0, 0 }, { 3, 0 }, 1, 1, 7, 1);
        CC_CHECK(!route.empty());
        CC_CHECK(route.front() == cc::IVec2(0, 0));
        CC_CHECK(route.back() == cc::IVec2(3, 0));
        // Occupied-by-other anchor routes out too.
        occ.ReserveFootprint({ 5, 5 }, 1, 1, 9, 1);
        const TilePath route2 = FindPathFootprint(map, occ, { 5, 5 }, { 7, 5 }, 1, 1, 7, 1);
        CC_CHECK(!route2.empty());
        CC_CHECK(route2.back() == cc::IVec2(7, 5));
        // Truly enclosed starts still fail closed (surrounded on all sides).
        OccupancyGrid box(3, 3);
        box.ReserveFootprint({ 0, 0 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 1, 0 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 2, 0 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 0, 1 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 2, 1 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 0, 2 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 1, 2 }, 1, 1, 9, 1);
        box.ReserveFootprint({ 2, 2 }, 1, 1, 9, 1);
        TileMap boxMap(3, 3);
        CC_CHECK(FindPathFootprint(boxMap, box, { 1, 1 }, { 0, 0 }, 1, 1, 7, 1).empty());
    }

    // --- Guard chase detour routes footprint-aware with occ, blind without ---
    {
        constexpr float kDt = 1.0f / 60.0f;
        auto makeDuel = []() {
            // chaser (0,0, team 0) vs prey (4,0 = 256px: out of 128px range,
            // inside 320px sight). Returns registry with both spawned.
            Registry registry;
            const Entity chaser = registry.Create();
            Unit hunter;
            hunter.type = UnitType::Infantry;
            ApplyBaseStats(hunter);
            hunter.teamID = 0;
            hunter.position = cc::ToRaylib(cc::TileToWorld(0, 0));
            registry.Add(chaser, hunter);
            const Entity prey = registry.Create();
            Unit victim;
            victim.type = UnitType::Infantry;
            ApplyBaseStats(victim);
            victim.teamID = 1;
            victim.position = cc::ToRaylib(cc::TileToWorld(4, 0));
            registry.Add(prey, victim);
            return std::make_tuple(std::move(registry), chaser, prey);
        };
        // With occ (prey anchor reserved like the live pre-pass): the detour
        // sanitizes to a neighbor and paths there.
        {
            auto [registry, chaser, prey] = makeDuel();
            OccupancyGrid occ(10, 10);
            TileMap map(10, 10);
            occ.ReserveFootprint({ 4, 0 }, 1, 1, prey, registry.Generation(prey));
            UpdateUnit(chaser, registry, map, kDt, nullptr, &occ);
            const Unit *hunter = registry.Get<Unit>(chaser);
            CC_CHECK(hunter->target == prey);
            CC_CHECK(hunter->hasPath);
            const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(hunter->moveTarget));
            CC_CHECK(!(dest == cc::IVec2(4, 0)));
            CC_CHECK(occ.CanEnter(map, dest, 1, 1, chaser, registry.Generation(chaser)));
            // Second tick with an unmoved target: no re-issue churn.
            const TilePath first = hunter->path;
            UpdateUnit(chaser, registry, map, kDt, nullptr, &occ);
            CC_CHECK(registry.Get<Unit>(chaser)->path == first);
        }
        // Without occ: legacy blind detour drives at the target tile.
        {
            auto [registry, chaser, prey] = makeDuel();
            TileMap map(10, 10);
            UpdateUnit(chaser, registry, map, kDt);
            const Unit *hunter = registry.Get<Unit>(chaser);
            CC_CHECK(hunter->target == prey);
            CC_CHECK(hunter->hasPath);
            CC_CHECK(cc::WorldToTile(cc::ToGlm(hunter->moveTarget)) == cc::IVec2(4, 0));
        }
    }
}
