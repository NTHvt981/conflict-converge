// Unit tests for the M14 skirmish build/teardown (headless match seeding +
// a short simulated match proving Start produces a living world).

#include "test_harness.h"

#include "AICommander.h"
#include "Building.h" // UpdateBaseIncome, HasFactory placement
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "MapFile.h"
#include "Menu.h" // TeamHasUnits
#include "Nodes.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Skirmish.h"
#include "TileMap.h"
#include "Unit.h" // UpdateUnit
#include "UnitFactory.h"

#include <string>

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

// 2v2 overflow: allied commander (team 0) + second enemy (team 1) wired
// into the bundle; 1v1 Harness above leaves them null by default.
struct Harness2v2
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
    AICommander allyAI{ registry, map, nodes, events, 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    AICommander enemyAI2{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    SkirmishWorld world{ &registry, &resources, &map, &occ, &fog, &nodes,
                         &queue,   &factory,   &ai, &allyAI, &enemyAI2, &camera, &rallyPos };
};

} // namespace

void RunSkirmishTests()
{
    // --- SpotsForMap: markers win, missing files fall back to legacy ---
    const std::string cross = ShippedMap("crossroads.map");
    CC_CHECK(!cross.empty());
    SkirmishSpots spots;
    if (!cross.empty())
    {
        spots = SpotsForMap(cross);
        MapData data;
        CC_CHECK(ParseMapFile(cross, data));
        CC_CHECK(spots.playerHome == data.playerSpawns[0]);
        CC_CHECK(spots.aiHome == data.aiSpawns[0]);
        bool ironFound = false;
        for (const MapNodeSpawn &spawn : data.nodes)
        {
            if (spawn.kind == ResourceKind::Iron && spawn.tile == spots.harvest)
            {
                ironFound = true;
            }
        }
        CC_CHECK(ironFound);
    }
    const SkirmishSpots legacy = SpotsForMap("no-such-map.map");
    CC_CHECK(legacy.playerHome == cc::IVec2(2, 2));
    CC_CHECK(legacy.aiHome == cc::IVec2(16, 9));

    // --- null bundle refused ---
    SkirmishWorld empty;
    CC_CHECK(!BuildSkirmish(empty, cross, AIDifficulty::Medium));

    // --- BuildSkirmish fields both sides headless ---
    Harness game;
    CC_CHECK(BuildSkirmish(game.world, cross, AIDifficulty::Hard));
    CC_CHECK(game.ai.Difficulty() == AIDifficulty::Hard);
    CC_CHECK(TeamHasUnits(game.registry, 0));
    CC_CHECK(TeamHasUnits(game.registry, 1));
    CC_CHECK(!game.queue.Empty());
    CC_CHECK(game.ai.HasFactory());
    CC_CHECK(game.resources.iron >= 0 && game.resources.oil >= 0);
    // Camera + rally aim at the player home.
    const Vector2 expectTarget =
        cc::ToRaylib(cc::TileToWorld(spots.playerHome.x, spots.playerHome.y) +
                     cc::Vec2(32.0f, 32.0f));
    CC_CHECK(game.camera.view.target.x == expectTarget.x);
    CC_CHECK(game.camera.view.target.y == expectTarget.y);
    const Vector2 expectRally =
        cc::ToRaylib(cc::TileToWorld(spots.playerHome.x + 4, spots.playerHome.y));
    CC_CHECK(game.rallyPos.x == expectRally.x);
    CC_CHECK(game.rallyPos.y == expectRally.y);

    // --- headless Start simulates: 10 seconds, both sides live ---
    for (int i = 0; i < 600; ++i)
    {
        const float dt = 1.0f / 60.0f;
        game.fog.Recompute(game.registry);
        game.registry.Each<Unit>([&](Entity id, Unit &unit) {
            UpdateUnit(id, game.registry, game.map, dt, &game.fog);
        });
        UpdateBaseIncome(game.registry, game.resources, dt, 0);
        game.nodes.Update(dt);
        game.nodes.GatherTick(game.registry, game.resources, dt, 0);
        if (game.ai.HasFactory())
        {
            game.queue.Update(game.factory, game.resources, 0, game.rallyPos, dt);
        }
        game.ai.Update(dt);
    }
    CC_CHECK(TeamHasUnits(game.registry, 0));
    CC_CHECK(TeamHasUnits(game.registry, 1));

    // --- ResetSkirmish drops the match (quit-to-menu teardown) ---
    ResetSkirmish(game.world);
    CC_CHECK(!TeamHasUnits(game.registry, 0));
    CC_CHECK(!TeamHasUnits(game.registry, 1));
    CC_CHECK(game.queue.Empty());
    CC_CHECK(game.resources.iron == 0 && game.resources.oil == 0);
    CC_CHECK(game.ai.WavesLaunched() == 0);
    CC_CHECK(game.ai.Difficulty() == AIDifficulty::Medium); // parked default
    ResetSkirmish(game.world); // safe on an empty world too
    CC_CHECK(true);

    // --- a fresh Build after Reset re-arms everything ---
    CC_CHECK(BuildSkirmish(game.world, cross, AIDifficulty::Easy));
    CC_CHECK(game.ai.Difficulty() == AIDifficulty::Easy);
    CC_CHECK(TeamHasUnits(game.registry, 0));
    CC_CHECK(TeamHasUnits(game.registry, 1));

    // --- BuildSandbox: terrain + one player Infantry, nothing else ---
    const std::string proto = ShippedMap("prototype.map");
    CC_CHECK(!proto.empty());
    if (!proto.empty())
    {
        MapData protoData;
        CC_CHECK(ParseMapFile(proto, protoData));
        CC_CHECK(!protoData.playerSpawns.empty());
        CC_CHECK(protoData.aiSpawns.empty()); // no '2' marker: sandbox signal
        CC_CHECK(protoData.width == 32 && protoData.height == 32);

        Harness sand;
        CC_CHECK(BuildSandbox(sand.world, proto));
        CC_CHECK(sand.map.Width() == 32 && sand.map.Height() == 32);
        // Exactly one unit: a team-0 PrototypeInfantry (infantry logic).
        int units = 0;
        bool protoFound = false;
        sand.registry.Each<Unit>([&](Entity, const Unit &unit) {
            ++units;
            protoFound = protoFound || (unit.type == UnitType::PrototypeInfantry &&
                                        unit.teamID == 0);
        });
        CC_CHECK(units == 1);
        CC_CHECK(protoFound);
        // No buildings, no enemy side, no funds, no queue.
        int buildings = 0;
        sand.registry.Each<Building>([&](Entity, const Building &) { ++buildings; });
        CC_CHECK(buildings == 0);
        CC_CHECK(!TeamHasUnits(sand.registry, 1));
        CC_CHECK(sand.resources.iron == 0 && sand.resources.oil == 0);
        CC_CHECK(sand.queue.Empty());
        // The squad parks on walkable ground at the marker.
        CC_CHECK(sand.map.Get({ 5, 16 }) != TerrainType::Water);
    }

    // --- BuildSandbox refuses maps without a player spawn ---
    {
        Harness sand;
        CC_CHECK(!BuildSandbox(sand.world, "data/does-not-exist.map"));
        CC_CHECK(!TeamHasUnits(sand.registry, 0));
    }

    // --- 2v2: twin-falls markers arm the allied + second enemy commanders ---
    const std::string falls = ShippedMap("twin_falls_2v2.map");
    CC_CHECK(!falls.empty());
    if (!falls.empty())
    {
        const SkirmishSpots f2 = SpotsForMap(falls);
        CC_CHECK(f2.is2v2);
        CC_CHECK(f2.allyHome == cc::IVec2(3, 16));
        CC_CHECK(f2.enemyHome2 == cc::IVec2(24, 16));
        // 1v1 maps stay off the 2v2 path.
        CC_CHECK(!SpotsForMap(cross).is2v2);

        Harness2v2 ally;
        CC_CHECK(BuildSkirmish(ally.world, falls, AIDifficulty::Easy));
        CC_CHECK(ally.allyAI.TeamID() == 0);
        CC_CHECK(ally.enemyAI2.TeamID() == 1);
        CC_CHECK(ally.allyAI.CombatUnitCount() > 0); // allied guard fielded
        CC_CHECK(ally.allyAI.HasFactory());          // allied base produces
        CC_CHECK(ally.enemyAI2.CombatUnitCount() > 0);
        CC_CHECK(ally.enemyAI2.HasFactory());
        CC_CHECK(TeamHasUnits(ally.registry, 0));
        CC_CHECK(TeamHasUnits(ally.registry, 1));
        for (int i = 0; i < 600; ++i)
        {
            const float dt = 1.0f / 60.0f;
            ally.fog.Recompute(ally.registry);
            ally.registry.Each<Unit>([&](Entity id, Unit &unit) {
                UpdateUnit(id, ally.registry, ally.map, dt, &ally.fog);
            });
            UpdateBaseIncome(ally.registry, ally.resources, dt, 0);
            ally.nodes.Update(dt);
            ally.nodes.GatherTick(ally.registry, ally.resources, dt, 0);
            if (ally.ai.HasFactory())
            {
                ally.queue.Update(ally.factory, ally.resources, 0, ally.rallyPos, dt);
            }
            ally.ai.Update(dt);
            ally.allyAI.Update(dt);
            ally.enemyAI2.Update(dt);
        }
        CC_CHECK(TeamHasUnits(ally.registry, 0));
        CC_CHECK(TeamHasUnits(ally.registry, 1));
        CC_CHECK(ally.allyAI.TeamID() == 0); // teams survive the sim
        CC_CHECK(ally.enemyAI2.TeamID() == 1);
        ResetSkirmish(ally.world);
        CC_CHECK(!TeamHasUnits(ally.registry, 0));
        CC_CHECK(!TeamHasUnits(ally.registry, 1));
        CC_CHECK(ally.allyAI.WavesLaunched() == 0);
        CC_CHECK(ally.enemyAI2.WavesLaunched() == 0);
        CC_CHECK(ally.allyAI.TeamID() == 0); // parked, not re-teamed
        CC_CHECK(ally.enemyAI2.TeamID() == 1);
    }
}
