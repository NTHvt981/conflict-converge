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
    const std::string candidates[] = { "data/" + name, "../../data/" + name,
                                       "../../../data/" + name };
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
    FogOfWar fog;
    ResourceNodes nodes;
    ProductionQueue queue;
    EventDispatcher events;
    UnitFactory factory{ registry, resources, events };
    GameCamera camera;
    Vector2 rallyPos = {};
    AICommander ai{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    SkirmishWorld world{ &registry, &resources, &map, &fog, &nodes,
                         &queue,   &factory,   &ai, &camera, &rallyPos };
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
            game.queue.Update(game.factory, 0, game.rallyPos, dt);
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
}
