// Regression coverage for stacked-spawn separation (Skirmish.cpp
// BuildSandbox): the sandbox spawns its squad stacked on the exact same
// tile (BuildSandbox computes the free tile once, reuses it for every
// SpawnPrepaid call), then this test sends the team-0 prototype squad to
// scattered destinations around the real prototype.map layout -- crossing
// paths around its water blob and tree cluster -- to catch
// collision/pathing regressions the same way movement_stall_tests.cpp does
// for hand-built maps, but against the actual shipped map and the real
// BuildSandbox spawn path. The test adapts to whatever the sandbox spawns
// (up to 6 units driven); spawn composition itself is not asserted here.

#include "test_harness.h"

#include "AICommander.h"
#include "Building.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "MapFile.h"
#include "Nodes.h"
#include "Pathfinder.h" // IssuePathOrderFootprint
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Skirmish.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitFactory.h"

#include <vector>

namespace
{

std::string ShippedMap(const std::string &name)
{
    const std::string candidates[] = { "data/maps/" + name, "../../data/maps/" + name,
                                       "../../../data/maps/" + name };
    for (const std::string &path : candidates)
    {
        MapData probe;
        if (ParseMapFile(path, probe))
        {
            return path;
        }
    }
    return "";
}

struct Harness
{
    Registry registry;
    ResourceSystem resources;
    TileMap map{ 20, 15 };
    OccupancyGrid occ{ 20, 15 };
    FogOfWar fog;
    ResourceNodes nodes;
    ProductionQueue queue;
    EventDispatcher events;
    UnitFactory factory{ registry, resources, events };
    GameCamera camera;
    Vector2 rallyPos = {};
    AICommander ai{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    SkirmishWorld world{ &registry, &resources, &map, &occ, &fog, &nodes,
                         &queue,   &factory,   &ai, nullptr, nullptr, &camera, &rallyPos };
};

} // namespace

void RunPrototypeSandboxMovementTests()
{
    const std::string proto = ShippedMap("prototype.map");
    CC_CHECK(!proto.empty());
    if (proto.empty())
    {
        return;
    }

    Harness sand;
    CC_CHECK(BuildSandbox(sand.world, proto));
    CC_CHECK(sand.map.Width() == 20 && sand.map.Height() == 10);

    std::vector<Entity> squad;
    sand.registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.type == UnitType::PrototypeInfantry && unit.teamID == 0)
        {
            squad.push_back(id);
        }
    });
    // Whatever the sandbox spawns, drive up to 6 of the team-0 prototypes
    // (more would overflow the destination list below; fewer just test less).
    CC_CHECK(!squad.empty());
    if (squad.empty())
    {
        return;
    }
    if (squad.size() > 6)
    {
        squad.resize(6);
    }

    // The squad spawns stacked at the exact same tile (see BuildSandbox).
    const cc::IVec2 spawnTile = cc::WorldToTile(cc::ToGlm(sand.registry.Get<Unit>(squad[0])->position));
    for (Entity id : squad)
    {
        CC_CHECK(cc::WorldToTile(cc::ToGlm(sand.registry.Get<Unit>(id)->position)) == spawnTile);
    }

    // 6 destinations spread around the 20x10 map's corners/mid-edges, well
    // clear of the spawn point and verified walkable (all Grass, never
    // Water) -- chosen so westward routes cross the tree cluster
    // cluster (~x2-6,y1-3) and southward routes press against the water blob
    // (~x12-19,y5-9), forcing real pathfinding + mutual collision pressure,
    // not just a straight unobstructed walk.
    const cc::IVec2 destinations[] = {
        { 1, 0 },   { 18, 0 },  { 1, 8 }, { 17, 4 }, { 5, 8 }, { 10, 5 },
    };
    for (const cc::IVec2 &dest : destinations)
    {
        CC_CHECK(!sand.map.IsBlocked(dest));
    }

    for (std::size_t i = 0; i < squad.size(); ++i)
    {
        Unit *u = sand.registry.Get<Unit>(squad[i]);
        IssuePathOrderFootprint(*u, sand.map, sand.occ, cc::ToRaylib(cc::TileToWorld(
                                                             destinations[i].x, destinations[i].y)),
                                squad[i], sand.registry.Generation(squad[i]));
        CC_CHECK(u->hasPath || u->hasMoveOrder); // path found, or straight-line fallback engaged
    }

    // Step the exact same per-frame pipeline the real game loop uses
    // (occupancy pre-pass, driver, continuous-space separation, stall
    // detection) until every unit's order resolves (arrival or a clean
    // stall-cancel) or the budget runs out. 40s is generous for a ~20-tile
    // diagonal crossing at foot speed with obstacle routing; the soak
    // tests already run into the 100s+ range for full matches.
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kBudget = 2400; // 40s at 60fps
    std::vector<bool> done(squad.size(), false);
    std::vector<int> doneFrame(squad.size(), -1);
    int frame = 0;
    for (; frame < kBudget; ++frame)
    {
        RunUnitMovementFrame(sand.registry, sand.map, sand.occ, &sand.fog, kDt);
        bool allDone = true;
        for (std::size_t i = 0; i < squad.size(); ++i)
        {
            const Unit *u = sand.registry.Get<Unit>(squad[i]);
            if (!done[i] && !u->hasMoveOrder && !u->hasPath)
            {
                done[i] = true;
                doneFrame[i] = frame;
            }
            allDone = allDone && done[i];
        }
        if (allDone)
        {
            break;
        }
    }

    // The core "collision issue" check: nothing stuck forever -- every
    // order must resolve (arrive or cleanly cancel) well within budget.
    for (std::size_t i = 0; i < squad.size(); ++i)
    {
        CC_CHECK(done[i]);
    }
    CC_CHECK(frame < kBudget);

    // Arrival check: destinations are fully walkable and reachable with no
    // dead-end (unlike movement_stall_tests.cpp's deliberate no-room
    // scenario). This used to need a generous stray-distance tolerance
    // because ReportSeparationStall's continuous-space push could cancel a
    // unit a few tiles short of an otherwise fully reachable goal (see
    // plans/UnitStackResolution_Plan.md) -- that mechanism is gone, so every
    // unit must now actually arrive exactly on its assigned tile. If this
    // regresses, it means ResolveStackedUnits (or ordinary occupancy
    // blocking) is introducing a new short-cancel failure mode.
    for (std::size_t i = 0; i < squad.size(); ++i)
    {
        const Unit *u = sand.registry.Get<Unit>(squad[i]);
        CC_CHECK(cc::WorldToTile(cc::ToGlm(u->position)) == destinations[i]);
    }

    // No two units end up wedged on the exact same tile after everything
    // has resolved (separation should have spread them out during transit
    // and at their distinct destinations).
    for (std::size_t i = 0; i < squad.size(); ++i)
    {
        for (std::size_t j = i + 1; j < squad.size(); ++j)
        {
            const Unit *ui = sand.registry.Get<Unit>(squad[i]);
            const Unit *uj = sand.registry.Get<Unit>(squad[j]);
            CC_CHECK(!(cc::WorldToTile(cc::ToGlm(ui->position)) ==
                      cc::WorldToTile(cc::ToGlm(uj->position))));
        }
    }
}
