// Unit tests for the skirmish build/teardown (headless match seeding +
// a short simulated match proving Start produces a living world).

#include "test_harness.h"

#include "units/AICommander.h"
#include "economy/Building.h" // UpdateBaseIncome, HasBootcamp placement
#include "core/Event.h"
#include "world/FogOfWar.h"
#include "app/ui/GameCamera.h"
#include "world/MapFile.h"
#include "app/match/Menu.h" // TeamHasUnits
#include "economy/Nodes.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/match/Skirmish.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h" // UpdateUnit
#include "units/UnitFactory.h"

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
    ProductionQueue bootcampQueue{ ProductionCategory::Infantry };
    ProductionQueue workshopQueue{ ProductionCategory::Vehicle };
    EventDispatcher events;
    UnitFactory factory{ registry, resources, events };
    GameCamera camera;
    AICommander ai{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    SkirmishWorld world{ &registry, &resources, &map, &occ, &fog, &nodes,
                         &bootcampQueue, &workshopQueue, &factory, &ai, nullptr, nullptr,
                         &camera };
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
    ProductionQueue bootcampQueue{ ProductionCategory::Infantry };
    ProductionQueue workshopQueue{ ProductionCategory::Vehicle };
    EventDispatcher events;
    UnitFactory factory{ registry, resources, events };
    GameCamera camera;
    AICommander ai{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    AICommander allyAI{ registry, map, nodes, events, 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    AICommander enemyAI2{ registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 } };
    SkirmishWorld world{ &registry, &resources, &map, &occ, &fog, &nodes,
                         &bootcampQueue, &workshopQueue, &factory, &ai, &allyAI, &enemyAI2,
                         &camera };
};

template <typename T>
Vector2 ProducerSpawnPos(T &game, BuildingType type)
{
    cc::IVec2 tile = { 2, 2 };
    const Entity producer = ProducerBuildingAt(game.registry, type, 0, 0);
    if (const Building *building = game.registry.Get<Building>(producer))
    {
        tile = BuildingSpawnTile(game.map, &game.occ, *building);
    }
    else
    {
        tile = NearestFreeFootprintTile(game.map, &game.occ, tile, 1, 1);
    }
    return cc::ToRaylib(cc::TileToWorld(tile.x, tile.y));
}

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
    CC_CHECK(!game.bootcampQueue.Empty());
    CC_CHECK(!game.workshopQueue.Empty());
    UpdateBuildingConstruction(game.registry, 20.0f); // sites -> Operational
    CC_CHECK(game.ai.HasBootcamp());
    CC_CHECK(game.ai.HasWorkshop());
    CC_CHECK(game.resources.iron >= 0 && game.resources.oil >= 0);
    // Camera aims at the player home.
    const Vector2 expectTarget =
        cc::ToRaylib(cc::TileToWorld(spots.playerHome.x, spots.playerHome.y) +
                     cc::Vec2(32.0f, 32.0f));
    CC_CHECK(game.camera.view.target.x == expectTarget.x);
    CC_CHECK(game.camera.view.target.y == expectTarget.y);

    // --- headless Start simulates: 10 seconds, both sides live ---
    for (int i = 0; i < 600; ++i)
    {
        const float dt = 1.0f / 60.0f;
        UpdateBuildingConstruction(game.registry, dt); // sites -> Operational, like Game
        game.fog.Recompute(game.registry);
        game.registry.Each<Unit>([&](Entity id, Unit &unit) {
            UpdateUnit(id, game.registry, game.map, dt, &game.fog);
        });
        UpdateBaseIncome(game.registry, game.resources, dt, 0);
        game.nodes.Update(dt);
        game.nodes.GatherTick(game.registry, game.resources, dt, 0);
        if (game.ai.HasBootcamp())
        {
            game.bootcampQueue.Update(game.factory, game.resources, 0,
                                      ProducerSpawnPos(game, BuildingType::Bootcamp), dt,
                                      &game.map, &game.occ);
        }
        if (game.ai.HasWorkshop())
        {
            game.workshopQueue.Update(game.factory, game.resources, 0,
                                      ProducerSpawnPos(game, BuildingType::Workshop), dt,
                                      &game.map, &game.occ);
        }
        game.ai.Update(dt);
    }
    CC_CHECK(TeamHasUnits(game.registry, 0));
    CC_CHECK(TeamHasUnits(game.registry, 1));

    // --- ResetSkirmish drops the match (quit-to-menu teardown) ---
    ResetSkirmish(game.world);
    CC_CHECK(!TeamHasUnits(game.registry, 0));
    CC_CHECK(!TeamHasUnits(game.registry, 1));
    CC_CHECK(game.bootcampQueue.Empty());
    CC_CHECK(game.workshopQueue.Empty());
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

    // --- BuildSandbox: terrain + player prototype squad, nothing else ---
    const std::string proto = ShippedMap("prototype.map");
    CC_CHECK(!proto.empty());
    if (!proto.empty())
    {
        MapData protoData;
        CC_CHECK(ParseMapFile(proto, protoData));
        CC_CHECK(!protoData.playerSpawns.empty());
        CC_CHECK(protoData.aiSpawns.empty()); // no '2' marker: sandbox signal
        CC_CHECK(protoData.width == 20 && protoData.height == 10);

        Harness sand;
        CC_CHECK(BuildSandbox(sand.world, proto));
        CC_CHECK(sand.map.Width() == 20 && sand.map.Height() == 10);
        // Sandbox seeds player-side foot units stacked at the same spawn
        // tile (BuildSandbox computes the free tile once and reuses it for
        // every SpawnPrepaid call). Composition is BuildSandbox's business:
        // this block only checks the match-level invariants below.
        // No buildings, no enemy side, no funds, no queue.
        int buildings = 0;
        sand.registry.Each<Building>([&](Entity, const Building &) { ++buildings; });
        CC_CHECK(buildings == 0);
        CC_CHECK(!TeamHasUnits(sand.registry, 1));
        CC_CHECK(sand.resources.iron == 0 && sand.resources.oil == 0);
        CC_CHECK(sand.bootcampQueue.Empty());
        CC_CHECK(sand.workshopQueue.Empty());
        // The squad parks on walkable ground at the marker (spawn tiles are
        // free by construction: BuildSandbox places units via NearestFreeTile
        // from the player spawn, so read the marker back instead of a
        // hardcoded tile that rots when the map is reworked).
        CC_CHECK(sand.map.Get(protoData.playerSpawns[0]) != TerrainType::Water);
    }

    // --- BuildSandbox refuses maps without a player spawn ---
    {
        Harness sand;
        CC_CHECK(!BuildSandbox(sand.world, "data/does-not-exist.map"));
        CC_CHECK(!TeamHasUnits(sand.registry, 0));
    }

    // --- prototype.map: producer rallies avoid footprints, spawns land on free tiles ---
    const std::string stuckMap = ShippedMap("prototype.map");
    CC_CHECK(!stuckMap.empty());
    if (!stuckMap.empty())
    {
        Harness stuck;
        CC_CHECK(BuildSkirmish(stuck.world, stuckMap, AIDifficulty::Easy));
        stuck.registry.Each<Building>([&](Entity, const Building &building) {
            if (building.teamID != 0 || !ProducerCategory(building.type).has_value())
            {
                return;
            }
            CC_CHECK(stuck.map.InBounds(building.rallyTile));
            CC_CHECK(!stuck.map.IsBlocked(building.rallyTile));
            const cc::IVec2 fp = Footprint(building.type);
            const bool rallyInFootprint =
                building.rallyTile.x >= building.tileX &&
                building.rallyTile.x < building.tileX + fp.x &&
                building.rallyTile.y >= building.tileY &&
                building.rallyTile.y < building.tileY + fp.y;
            CC_CHECK(!rallyInFootprint);
        });
        for (int i = 0; i < 1200; ++i)
        {
            const float dt = 1.0f / 60.0f;
            UpdateBuildingConstruction(stuck.registry, dt);
            stuck.fog.Recompute(stuck.registry);
            stuck.registry.Each<Unit>([&](Entity id, Unit &unit) {
                UpdateUnit(id, stuck.registry, stuck.map, dt, &stuck.fog);
            });
            UpdateBaseIncome(stuck.registry, stuck.resources, dt, 0);
            stuck.nodes.Update(dt);
            stuck.nodes.GatherTick(stuck.registry, stuck.resources, dt, 0);
            stuck.bootcampQueue.Update(stuck.factory, stuck.resources, 0,
                                       ProducerSpawnPos(stuck, BuildingType::Bootcamp), dt,
                                       &stuck.map, &stuck.occ);
            stuck.workshopQueue.Update(stuck.factory, stuck.resources, 0,
                                       ProducerSpawnPos(stuck, BuildingType::Workshop), dt,
                                       &stuck.map, &stuck.occ);
        }
        bool allFree = true;
        stuck.registry.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.teamID != 0)
            {
                return;
            }
            if (stuck.map.IsBlocked(cc::WorldToTile(cc::ToGlm(unit.position))))
            {
                allFree = false;
            }
        });
        CC_CHECK(allFree);
        CC_CHECK(TeamHasUnits(stuck.registry, 0));
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
        UpdateBuildingConstruction(ally.registry, 20.0f); // sites -> Operational
        CC_CHECK(ally.allyAI.HasBootcamp());          // allied base produces
        CC_CHECK(ally.allyAI.HasWorkshop());
        CC_CHECK(ally.enemyAI2.CombatUnitCount() > 0);
        CC_CHECK(ally.enemyAI2.HasBootcamp());
        CC_CHECK(ally.enemyAI2.HasWorkshop());
        CC_CHECK(TeamHasUnits(ally.registry, 0));
        CC_CHECK(TeamHasUnits(ally.registry, 1));
        for (int i = 0; i < 600; ++i)
        {
            const float dt = 1.0f / 60.0f;
            UpdateBuildingConstruction(ally.registry, dt); // sites -> Operational, like Game
            ally.fog.Recompute(ally.registry);
            ally.registry.Each<Unit>([&](Entity id, Unit &unit) {
                UpdateUnit(id, ally.registry, ally.map, dt, &ally.fog);
            });
            UpdateBaseIncome(ally.registry, ally.resources, dt, 0);
            ally.nodes.Update(dt);
            ally.nodes.GatherTick(ally.registry, ally.resources, dt, 0);
            if (ally.ai.HasBootcamp())
            {
                ally.bootcampQueue.Update(ally.factory, ally.resources, 0,
                                          ProducerSpawnPos(ally, BuildingType::Bootcamp), dt,
                                          &ally.map, &ally.occ);
            }
            if (ally.ai.HasWorkshop())
            {
                ally.workshopQueue.Update(ally.factory, ally.resources, 0,
                                          ProducerSpawnPos(ally, BuildingType::Workshop), dt,
                                          &ally.map, &ally.occ);
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
