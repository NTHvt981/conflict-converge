// Tests for plans/StackedOrderDeadlock_Plan.md's two fixes:
//   Phase 1 (Formation.cpp): units sharing a start tile at formation-move
//   issue time get staggered (one keeper issued directly, the rest get an
//   escape hop + their real slot queued behind it) so nobody contests the
//   shared tile and nobody's order gets silently cancelled.
//   Phase 2 (Unit.cpp ResolveStackedUnits): a stack member whose order has
//   already exhausted its blocked-retry budget gets rescued with a fresh
//   order instead of being left to go idle after a silent cancel.
// formation_stack_move_tests.cpp already covers "eventually completes";
// these tests specifically prove there is no multi-second stall window at
// all, and that units reach their ORIGINAL requested destinations (not an
// anonymous rescue tile).

#include "test_harness.h"

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

// Steps the real per-frame pipeline until every unit's order resolves (or
// the budget runs out), returning the frame it took. `maxStillFrames` is
// tracked so callers can assert nobody sat with zero velocity for more
// than a couple of frames -- the whole point of Phase 1's fix.
struct RunResult
{
    int completedAtFrame = -1;
    int maxConsecutiveStillFrames = 0;
};

RunResult RunUntilDone(Registry &registry, TileMap &map, OccupancyGrid &occ,
                       const std::vector<Entity> &squad, int budget)
{
    RunResult result;
    std::vector<int> stillStreak(squad.size(), 0);
    constexpr float kDt = 1.0f / 60.0f;
    for (int frame = 0; frame < budget; ++frame)
    {
        RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
        bool allDone = true;
        for (std::size_t i = 0; i < squad.size(); ++i)
        {
            const Unit *u = registry.Get<Unit>(squad[i]);
            allDone = allDone && !u->hasMoveOrder && !u->hasPath;
            const bool active = u->hasMoveOrder || u->hasPath;
            const bool still = u->velocity.x == 0.0f && u->velocity.y == 0.0f;
            if (active && still)
            {
                ++stillStreak[i];
                if (stillStreak[i] > result.maxConsecutiveStillFrames)
                {
                    result.maxConsecutiveStillFrames = stillStreak[i];
                }
            }
            else
            {
                stillStreak[i] = 0;
            }
        }
        if (allDone)
        {
            result.completedAtFrame = frame;
            break;
        }
    }
    return result;
}
} // namespace

void RunFormationDeadlockFixTests()
{
    // --- Phase 1: 2 units on the exact same tile, distinct destinations ---
    // This is the exact live-reported scenario. Before the fix, both would
    // block each other for ~1.5-2s (90-120 frames) before one got silently
    // cancelled and rerouted by ResolveStackedUnits. After the fix, neither
    // should ever sit still for more than a couple of frames, and both
    // should reach their ORIGINAL requested destinations.
    {
        TileMap map(30, 30);
        OccupancyGrid occ(30, 30);
        Registry registry;
        const Entity a = registry.Create();
        registry.Add(a, MakeInfantry(10, 10));
        const Entity b = registry.Create();
        registry.Add(b, MakeInfantry(10, 10));
        const std::vector<Entity> squad{ a, b };

        formation::IssueFormationMoveFP(registry, squad, map, occ,
                                        cc::ToRaylib(cc::TileToWorld(20, 10)), false);
        // Both must have an active order the instant the order is issued --
        // one direct (the keeper), one via the escape hop (the straggler).
        for (Entity id : squad)
        {
            const Unit *u = registry.Get<Unit>(id);
            CC_CHECK(u->hasMoveOrder || u->hasPath);
        }
        // The formation slots for a 2-unit group (cellSize 1) are
        // (20,10) and (21,10) -- record them before stepping so the final
        // check doesn't depend on re-deriving FormationOffsetsFP here.
        const cc::IVec2 slotA{ 20, 10 };
        const cc::IVec2 slotB{ 21, 10 };

        const RunResult result = RunUntilDone(registry, map, occ, squad, 2400);
        CC_CHECK(result.completedAtFrame >= 0); // never freezes forever
        // The heart of Phase 1: no unit should ever sit motionless with an
        // active order for more than a handful of frames (a couple of
        // frames of legitimate per-step rounding is fine; ~90+ frames is
        // the old silent-cancel stall this fix removes).
        CC_CHECK(result.maxConsecutiveStillFrames < 10);

        const cc::IVec2 tileA = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(a)->position));
        const cc::IVec2 tileB = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(b)->position));
        // Each unit's real destination is tied to its own index in the
        // `squad` list passed to IssueFormationMoveFP (offsets[i]), not to
        // whether it ends up the keeper or the staggered straggler -- so
        // the pairing is exact, not either/or. Both must land on their real
        // slot, not some anonymous nearby tile from an old-style rescue
        // reroute.
        CC_CHECK(tileA == slotA);
        CC_CHECK(tileB == slotB);
    }

    // --- Phase 1: 4 units on the exact same tile (keeper + 3 staggered) ---
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

        const RunResult result = RunUntilDone(registry, map, occ, squad, 2400);
        CC_CHECK(result.completedAtFrame >= 0);
        CC_CHECK(result.maxConsecutiveStillFrames < 10);

        std::vector<cc::IVec2> finalTiles;
        for (Entity id : squad)
        {
            const Unit *u = registry.Get<Unit>(id);
            finalTiles.push_back(cc::WorldToTile(cc::ToGlm(u->position)));
        }
        for (std::size_t i = 0; i < finalTiles.size(); ++i)
        {
            for (std::size_t j = i + 1; j < finalTiles.size(); ++j)
            {
                CC_CHECK(!(finalTiles[i] == finalTiles[j]));
            }
        }
    }

    // --- Phase 2: ResolveStackedUnits rescues a deadlocked ordered unit ---
    // Simulates the exact moment a unit's order is one blocked frame away
    // from a silent cancel (blockedRepaths already at the retry ceiling)
    // while sharing a tile with another ordered unit -- a case Phase 1
    // can't reach (both already have orders; this isn't the order-issue
    // moment). Before this fix, ResolveStackedUnits would see both as
    // "order in flight" and leave the stack alone entirely.
    {
        TileMap map(20, 20);
        OccupancyGrid occ(20, 20);
        Registry registry;
        const Entity a = registry.Create();
        registry.Add(a, MakeInfantry(8, 8));
        const Entity b = registry.Create();
        registry.Add(b, MakeInfantry(8, 8));

        Unit *ua = registry.Get<Unit>(a);
        Unit *ub = registry.Get<Unit>(b);
        // Both "have an order" but neither has moved (as if each is
        // blocking the other's very first step). Give A a maxed-out retry
        // budget -- one more blocked frame would silently cancel it.
        ua->hasMoveOrder = true;
        ua->moveTarget = cc::ToRaylib(cc::TileToWorld(15, 8));
        ua->blockedRepaths = 3; // matches Unit.cpp's private kMaxBlockedRepaths
        ua->blockedTime = 0.1f; // currently mid-block, not just a past history of repaths
        ub->hasMoveOrder = true;
        ub->moveTarget = cc::ToRaylib(cc::TileToWorld(8, 15));
        ub->blockedRepaths = 0; // B's order is still healthy

        // Seed occupancy the way RunUnitMovementFrame's pre-pass would.
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
            occ.ReleaseFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                      registry.Generation(id));
            (void)occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                            registry.Generation(id));
        });
        // Whichever of the two actually won the tile's single-owner
        // reservation is never eligible to be picked (NearestEnterableTile
        // would see the tile as empty from the holder's own perspective --
        // see the long comment in ResolveStackedUnits). So: if A happens to
        // be the holder, the FIX must relocate B instead (a healthy
        // non-holder) to free the tile for A's own next attempt -- it must
        // NOT try to move A. If A is not the holder, A itself gets rescued
        // directly. Determine which case this run landed in rather than
        // assuming -- ReserveFootprintOwned's first-come winner isn't part
        // of this test's contract.
        const OccEntry holder = occ.GetUnit(cc::WorldToTile(cc::ToGlm(ua->position)));
        const bool aIsHolder = holder.entity == a && holder.generation == registry.Generation(a);

        ResolveStackedUnits(registry, map, occ);

        if (aIsHolder)
        {
            // A is never touched -- its own order (stale moveTarget,
            // maxed-out blockedRepaths) is left exactly as-is; the fix
            // instead relocates B to free the tile.
            CC_CHECK(ua->moveTarget.x == cc::ToRaylib(cc::TileToWorld(15, 8)).x &&
                    ua->moveTarget.y == cc::ToRaylib(cc::TileToWorld(15, 8)).y);
            CC_CHECK(ub->hasPath);
            const cc::IVec2 rescueDest = cc::WorldToTile(cc::ToGlm(ub->moveTarget));
            CC_CHECK(!(rescueDest == cc::IVec2(8, 8)));
        }
        else
        {
            // A (deadlocked, not the holder) gets rescued directly: a
            // fresh path toward a real, different tile -- not left with its
            // stale moveTarget and doomed to the next silent cancel.
            CC_CHECK(ua->hasPath);
            const cc::IVec2 rescueDest = cc::WorldToTile(cc::ToGlm(ua->moveTarget));
            CC_CHECK(!(rescueDest == cc::IVec2(8, 8)));
            // B's healthy order must be completely untouched.
            CC_CHECK(ub->moveTarget.x == cc::ToRaylib(cc::TileToWorld(8, 15)).x &&
                    ub->moveTarget.y == cc::ToRaylib(cc::TileToWorld(8, 15)).y);
            CC_CHECK(ub->blockedRepaths == 0);
        }
    }
}
