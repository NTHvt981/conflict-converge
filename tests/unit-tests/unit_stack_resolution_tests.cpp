// Unit tests for ResolveStackedUnits (Unit.cpp), which replaced the old
// continuous-space SeparateUnits push + crush damage (see
// plans/UnitStackResolution_Plan.md). Detects units sharing the exact same
// anchor tile and relocates one per stack per call via a completely
// ordinary footprint-aware move order -- no per-unit collision flag, no
// state machine.

#include "test_harness.h"

#include "Pathfinder.h" // IssuePathOrderFootprint (not used directly, transitively needed)
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

namespace
{

Unit MakeUnit(int tileX, int tileY, UnitType type = UnitType::Infantry)
{
    Unit unit;
    unit.type = type;
    ApplyBaseStats(unit); // sets footprintWidth/Height (2x2 for vehicles)
    unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    return unit;
}

// ResolveStackedUnits reads OccupancyGrid to tell "the tile's current
// reservation holder" apart from everyone else sharing it (see the picking
// logic's own comment in Unit.cpp) -- it does not populate occ itself.
// RunUnitMovementFrame's pre-pass normally does this before calling it every
// frame; tests calling ResolveStackedUnits directly (not through the full
// pipeline) must reproduce that pre-pass themselves or the grid stays empty
// and every unit looks like it has no neighbor.
void SeedOccupancy(Registry &registry, OccupancyGrid &occ)
{
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.health > 0.0f)
        {
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
            occ.ReleaseFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                      registry.Generation(id));
            (void)occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                            registry.Generation(id));
        }
    });
}

} // namespace

void RunUnitStackResolutionTests()
{
    // --- two units on the same tile, no orders: exactly one gets relocated ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity a = registry.Create();
        registry.Add(a, MakeUnit(5, 5));
        const Entity b = registry.Create();
        registry.Add(b, MakeUnit(5, 5));

        SeedOccupancy(registry, occ);
        ResolveStackedUnits(registry, map, occ);

        const Unit *ua = registry.Get<Unit>(a);
        const Unit *ub = registry.Get<Unit>(b);
        const bool aActive = ua->hasMoveOrder || ua->hasPath;
        const bool bActive = ub->hasMoveOrder || ub->hasPath;
        CC_CHECK(aActive != bActive); // exactly one, not both, not neither

        // A second call in the same frame must not also pick the other one
        // (per-stack serialize: a relocation already in flight leaves the
        // stack alone).
        SeedOccupancy(registry, occ);
        ResolveStackedUnits(registry, map, occ);
        const bool aActive2 = registry.Get<Unit>(a)->hasMoveOrder || registry.Get<Unit>(a)->hasPath;
        const bool bActive2 = registry.Get<Unit>(b)->hasMoveOrder || registry.Get<Unit>(b)->hasPath;
        CC_CHECK(aActive2 != bActive2);
        CC_CHECK(aActive == aActive2 && bActive == bActive2); // same one stays picked
    }

    // --- stepped via the real per-frame pipeline: both end up separated ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity a = registry.Create();
        registry.Add(a, MakeUnit(5, 5));
        const Entity b = registry.Create();
        registry.Add(b, MakeUnit(5, 5));

        constexpr float kDt = 1.0f / 60.0f;
        int frame = 0;
        for (; frame < 300; ++frame)
        {
            RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
            const Unit *ua = registry.Get<Unit>(a);
            const Unit *ub = registry.Get<Unit>(b);
            if (!ua->hasMoveOrder && !ua->hasPath && !ub->hasMoveOrder && !ub->hasPath)
            {
                break;
            }
        }
        CC_CHECK(frame < 300);
        const cc::IVec2 tileA = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(a)->position));
        const cc::IVec2 tileB = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(b)->position));
        CC_CHECK(!(tileA == tileB));
    }

    // --- 2x2 vehicle stack: the destination must actually fit the footprint ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity a = registry.Create();
        registry.Add(a, MakeUnit(5, 5, UnitType::HeavyTank));
        const Entity b = registry.Create();
        registry.Add(b, MakeUnit(5, 5, UnitType::HeavyTank));
        CC_CHECK(registry.Get<Unit>(a)->footprintWidth == 2 &&
                registry.Get<Unit>(a)->footprintHeight == 2);

        SeedOccupancy(registry, occ);
        ResolveStackedUnits(registry, map, occ);

        const Unit *ua = registry.Get<Unit>(a);
        const Unit *ub = registry.Get<Unit>(b);
        const Entity picked = (ua->hasMoveOrder || ua->hasPath) ? a : b;
        const Unit *pickedUnit = registry.Get<Unit>(picked);
        CC_CHECK(pickedUnit->hasMoveOrder || pickedUnit->hasPath);
        const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(pickedUnit->moveTarget));
        CC_CHECK(!(dest == cc::IVec2(5, 5))); // actually moved off the stack tile
        // The chosen destination must genuinely fit the full 2x2 footprint,
        // not just a single free tile (NearestFreeTile-style would be wrong
        // here -- see the plan's correction re: NearestEnterableTile).
        CC_CHECK(occ.CanEnter(map, dest, 2, 2, picked, registry.Generation(picked)));
    }

    // --- two independent stacks resolved in the same call: both progress ---
    {
        TileMap map(20, 20);
        OccupancyGrid occ(20, 20);
        Registry registry;
        const Entity a0 = registry.Create();
        registry.Add(a0, MakeUnit(2, 2));
        const Entity a1 = registry.Create();
        registry.Add(a1, MakeUnit(2, 2));
        const Entity b0 = registry.Create();
        registry.Add(b0, MakeUnit(17, 17));
        const Entity b1 = registry.Create();
        registry.Add(b1, MakeUnit(17, 17));

        ResolveStackedUnits(registry, map, occ);

        auto stackProgressed = [&](Entity x, Entity y) {
            const Unit *ux = registry.Get<Unit>(x);
            const Unit *uy = registry.Get<Unit>(y);
            const bool xActive = ux->hasMoveOrder || ux->hasPath;
            const bool yActive = uy->hasMoveOrder || uy->hasPath;
            return xActive != yActive;
        };
        CC_CHECK(stackProgressed(a0, a1)); // near (2,2) resolved
        CC_CHECK(stackProgressed(b0, b1)); // near (17,17) resolved, independently
    }

    // --- a solo unit with an active order is left completely untouched ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity solo = registry.Create();
        registry.Add(solo, MakeUnit(1, 1));
        Unit *unit = registry.Get<Unit>(solo);
        IssueMoveOrder(*unit, cc::ToRaylib(cc::TileToWorld(8, 8)));
        const Vector2 targetBefore = unit->moveTarget;
        const bool hasMoveBefore = unit->hasMoveOrder;

        ResolveStackedUnits(registry, map, occ);

        const Unit *after = registry.Get<Unit>(solo);
        CC_CHECK(after->hasMoveOrder == hasMoveBefore);
        CC_CHECK(after->moveTarget.x == targetBefore.x && after->moveTarget.y == targetBefore.y);
    }

    // --- regression (e2e InBounds assert): stacked units with off-map
    // positions must not reach OccupancyGrid::GetUnit ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity a = registry.Create();
        registry.Add(a, MakeUnit(5, 5));
        const Entity b = registry.Create();
        registry.Add(b, MakeUnit(5, 5));
        // Past the east edge: tile.x = 10 is OOB on a 10-wide map.
        registry.Get<Unit>(a)->position = Vector2{ 10.0f * 64.0f + 1.0f, 5.0f * 64.0f };
        registry.Get<Unit>(b)->position = Vector2{ 10.0f * 64.0f + 1.0f, 5.0f * 64.0f };

        SeedOccupancy(registry, occ);
        ResolveStackedUnits(registry, map, occ); // used to assert InBounds in GetUnit

        // OOB stacks are skipped, not relocated.
        CC_CHECK(!registry.Get<Unit>(a)->hasMoveOrder && !registry.Get<Unit>(a)->hasPath);
        CC_CHECK(!registry.Get<Unit>(b)->hasMoveOrder && !registry.Get<Unit>(b)->hasPath);
    }

    // --- regression: an off-map move target within one step cancels
    // in-bounds instead of teleporting the unit out of bounds (the e2e
    // real-dt spike path: StepToward's arrive shortcut skipped IsBlocked) ---
    {
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        Registry registry;
        const Entity solo = registry.Create();
        registry.Add(solo, MakeUnit(9, 5));
        Unit *unit = registry.Get<Unit>(solo);
        unit->speed = 200.0f;
        IssueMoveOrder(*unit, Vector2{ 10.0f * 64.0f + 32.0f, 5.0f * 64.0f }); // OOB target
        UpdateUnitMovement(*unit, map, EffectiveSpeed(*unit), 1.0f, &occ, solo,
                           registry.Generation(solo));
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit->position));
        CC_CHECK(map.InBounds(tile));
        CC_CHECK(!unit->hasMoveOrder && !unit->hasPath);
    }
}
