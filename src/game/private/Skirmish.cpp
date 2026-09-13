#include "Skirmish.h"

#include "Building.h"      // PlaceBuilding, BuildingType
#include "FogOfWar.h"      // Resize
#include "GameCamera.h"    // view target
#include "MapFile.h"       // ParseMapFile, ApplyMapData, NearestFreeTile
#include "Nodes.h"         // SpawnNode, ResourceKind
#include "Production.h"    // Enqueue
#include "Registry.h"      // Clear
#include "ResourceSystem.h" // starting funds
#include "TileMap.h"       // Resize, Set
#include "Unit.h"          // UnitType
#include "UnitFactory.h"   // Spawn

SkirmishSpots SpotsForMap(const std::string &mapPath)
{
    SkirmishSpots spots;
    MapData data;
    if (!ParseMapFile(mapPath, data))
    {
        return spots;
    }
    if (!data.playerSpawns.empty())
    {
        spots.playerHome = data.playerSpawns[0];
    }
    if (!data.aiSpawns.empty())
    {
        spots.aiHome = data.aiSpawns[0];
    }
    for (const MapNodeSpawn &spawn : data.nodes)
    {
        if (spawn.kind == ResourceKind::Iron)
        {
            spots.harvest = spawn.tile;
            break;
        }
    }
    return spots;
}

void ResetSkirmish(SkirmishWorld &world)
{
    if (world.registry != nullptr)
    {
        world.registry->Clear();
    }
    if (world.resources != nullptr)
    {
        *world.resources = ResourceSystem();
    }
    if (world.nodes != nullptr)
    {
        *world.nodes = ResourceNodes();
    }
    if (world.queue != nullptr)
    {
        *world.queue = ProductionQueue();
    }
    if (world.ai != nullptr)
    {
        // Park the commander without a base: the next Build re-arms it via
        // Reset + SetupBase, while the load path re-arms it bare (the loaded
        // world already fields the AI side).
        world.ai->Reset(AIDifficulty::Medium, { 0, 0 }, { 0, 0 });
    }
}

bool BuildSkirmish(SkirmishWorld &world, const std::string &mapPath, AIDifficulty difficulty)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.fog == nullptr || world.nodes == nullptr || world.queue == nullptr ||
        world.factory == nullptr || world.ai == nullptr || world.camera == nullptr ||
        world.rallyPos == nullptr)
    {
        return false;
    }
    ResetSkirmish(world);

    Registry &registry = *world.registry;
    ResourceSystem &resources = *world.resources;
    TileMap &map = *world.map;
    FogOfWar &fog = *world.fog;
    ResourceNodes &nodes = *world.nodes;
    ProductionQueue &queue = *world.queue;
    UnitFactory &factory = *world.factory;

    // Same starting funds both sides see (Q74 fair rules; the AI seeds its
    // own 1000/500 inside SetupBase below).
    resources.AddIron(1000);
    resources.AddOil(500);

    const SkirmishSpots spots = SpotsForMap(mapPath);
    MapData data;
    if (ParseMapFile(mapPath, data))
    {
        ApplyMapData(data, map, nodes);
    }
    else
    {
        // Legacy hardcoded layout when the file is missing.
        map.Resize(20, 15);
        map.Set({ 6, 3 }, TerrainType::Water);
        map.Set({ 7, 3 }, TerrainType::Water);
        map.Set({ 6, 4 }, TerrainType::Water);
        nodes.SpawnNode(map, ResourceKind::Iron, { 15, 3 }, 200.0f, 10.0f);
        nodes.SpawnNode(map, ResourceKind::Oil, { 15, 12 }, 150.0f, 10.0f);
    }
    fog.Resize(map.Width(), map.Height());

    auto spawnDemo = [&](UnitType type, int tileX, int tileY, int team) {
        // Spawn on walkable ground: the marker itself may sit inside the
        // freshly placed base footprint (trapped units otherwise).
        const cc::IVec2 free = NearestFreeTile(map, tileX, tileY);
        factory.Spawn(type, team, cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
    };
    auto placeBase = [&](int team, cc::IVec2 anchor) {
        PlaceBuilding(registry, map, BuildingType::Base, team, anchor.x, anchor.y);
        const cc::IVec2 depotSpots[] = { { 2, 0 }, { 0, 2 }, { -1, 0 } };
        for (const cc::IVec2 &spot : depotSpots)
        {
            if (PlaceBuilding(registry, map, BuildingType::ResourceDepot, team, anchor.x + spot.x,
                              anchor.y + spot.y) != kInvalidEntity)
            {
                break;
            }
        }
        const cc::IVec2 factorySpots[] = { { 0, 2 }, { 3, 0 }, { -2, 2 } };
        for (const cc::IVec2 &spot : factorySpots)
        {
            if (PlaceBuilding(registry, map, BuildingType::Factory, team, anchor.x + spot.x,
                              anchor.y + spot.y) != kInvalidEntity)
            {
                break;
            }
        }
    };
    // NOTE: bases go down BEFORE units spawn, so NearestFreeTile routes
    // around footprints instead of trapping units inside them.
    placeBase(0, spots.playerHome);
    spawnDemo(UnitType::Infantry, spots.playerHome.x, spots.playerHome.y, 0);
    spawnDemo(UnitType::LightTank, spots.playerHome.x + 2, spots.playerHome.y, 0);
    spawnDemo(UnitType::Artillery, spots.playerHome.x + 1, spots.playerHome.y + 3, 0);
    spawnDemo(UnitType::Engineer, spots.harvest.x, spots.harvest.y, 0); // harvester on iron
    queue.Enqueue(resources, UnitType::Infantry);
    queue.Enqueue(resources, UnitType::LightTank);

    // Enemy commander owns team 1 under fair rules. Its starting guard keeps
    // team 1 fielded from frame one so the outcome check never fires instantly.
    world.ai->Reset(difficulty, spots.aiHome, spots.playerHome);
    world.ai->SetupBase();

    *world.rallyPos = cc::ToRaylib(cc::TileToWorld(spots.playerHome.x + 4, spots.playerHome.y));
    world.camera->view.target = cc::ToRaylib(
        cc::TileToWorld(spots.playerHome.x, spots.playerHome.y) + cc::Vec2(32.0f, 32.0f));
    return true;
}
