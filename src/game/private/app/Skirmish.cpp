#include "Skirmish.h"

#include "Building.h"      // PlaceBuilding, BuildingType
#include "FogOfWar.h"      // Resize
#include "GameCamera.h"    // view target
#include "MapFile.h"       // ParseMapFile, ApplyMapData, NearestFreeTile
#include "Nodes.h"         // SpawnNode, ResourceKind
#include "Production.h"    // Enqueue
#include "Registry.h"      // Clear
#include "ResourceSystem.h" // starting funds
#include "TileMap.h"       // Resize, Set, OccupancyGrid
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
    // 2v2: second markers per side arm the allied + second enemy commanders.
    // Fallbacks keep single-pair maps on the 1v1 path (is2v2 false).
    if (data.playerSpawns.size() >= 2 && data.aiSpawns.size() >= 2)
    {
        spots.allyHome = data.playerSpawns[1];
        spots.enemyHome2 = data.aiSpawns[1];
        spots.is2v2 = true;
    }
    else
    {
        spots.allyHome = { spots.playerHome.x, spots.playerHome.y + 10 };
        spots.enemyHome2 = { spots.aiHome.x, spots.aiHome.y - 4 };
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
    if (world.occ != nullptr)
    {
        world.occ->Clear();
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
        world.ai->Reset(AIDifficulty::Medium, { 0, 0 }, { 0, 0 }, 1);
    }
    if (world.allyAI != nullptr)
    {
        world.allyAI->Reset(AIDifficulty::Medium, { 0, 0 }, { 0, 0 }, 0);
    }
    if (world.enemyAI2 != nullptr)
    {
        world.enemyAI2->Reset(AIDifficulty::Medium, { 0, 0 }, { 0, 0 }, 1);
    }
    // Bind shared occupancy so AI orders route footprint-aware. Identity is
    // stable across matches (members, never reallocated), so binding here
    // covers Build, menu Start, and the load path — all funnel through this
    // reset. Null grid (bare tests) keeps legacy blind orders.
    if (world.ai != nullptr)
    {
        world.ai->SetOccupancy(world.occ);
    }
    if (world.allyAI != nullptr)
    {
        world.allyAI->SetOccupancy(world.occ);
    }
    if (world.enemyAI2 != nullptr)
    {
        world.enemyAI2->SetOccupancy(world.occ);
    }
}

bool BuildSkirmish(SkirmishWorld &world, const std::string &mapPath, AIDifficulty difficulty)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.occ == nullptr || world.fog == nullptr || world.nodes == nullptr ||
        world.queue == nullptr || world.factory == nullptr || world.ai == nullptr ||
        world.camera == nullptr || world.rallyPos == nullptr)
    {
        return false;
    }
    ResetSkirmish(world);

    Registry &registry = *world.registry;
    ResourceSystem &resources = *world.resources;
    TileMap &map = *world.map;
    OccupancyGrid &occ = *world.occ;
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
        ApplyMapData(data, map, nodes, &occ);
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
    occ.Resize(map.Width(), map.Height());

    auto spawnDemo = [&](UnitType type, int tileX, int tileY, int team) {
        // Spawn on walkable ground: the marker itself may sit inside the
        // freshly placed base footprint (trapped units otherwise).
        const cc::IVec2 free = NearestFreeTile(map, tileX, tileY);
        factory.Spawn(type, team, cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
    };
    auto placeBase = [&](int team, cc::IVec2 anchor) {
        // Reserve a fresh footprint in the occupancy grid.
        auto reserve = [&](cc::IVec2 at, BuildingType type, Entity entity) {
            const cc::IVec2 fp = Footprint(type);
            occ.ReserveFootprint(at, fp.x, fp.y, entity, registry.Generation(entity));
        };
        // Try one candidate site; returns kInvalidEntity when blocked.
        auto trySite = [&](BuildingType type, cc::IVec2 at) -> Entity {
            const Entity e =
                PlaceBuilding(registry, map, type, team, at.x, at.y, &nodes);
            if (e != kInvalidEntity)
            {
                reserve(at, type, e);
            }
            return e;
        };
        // Spiral fallback (rings 1..6) when every preferred spot is
        // blocked: an adversarial map must not silently leave a side
        // without production.
        auto trySpiral = [&](BuildingType type, cc::IVec2 center) -> Entity {
            for (int ring = 1; ring <= 6; ++ring)
            {
                for (int dy = -ring; dy <= ring; ++dy)
                {
                    for (int dx = -ring; dx <= ring; ++dx)
                    {
                        const Entity e = trySite(type, { center.x + dx, center.y + dy });
                        if (e != kInvalidEntity)
                        {
                            return e;
                        }
                    }
                }
            }
            return kInvalidEntity;
        };
        // Base first at the preferred anchor, else wherever fits nearby.
        // Depot/factory cluster around wherever the base actually landed.
        cc::IVec2 home = anchor;
        bool basePlaced = trySite(BuildingType::Base, home) != kInvalidEntity;
        for (int ring = 1; !basePlaced && ring <= 6; ++ring)
        {
            for (int dy = -ring; !basePlaced && dy <= ring; ++dy)
            {
                for (int dx = -ring; dx <= ring; ++dx)
                {
                    if (trySite(BuildingType::Base, { anchor.x + dx, anchor.y + dy }) !=
                        kInvalidEntity)
                    {
                        home = { anchor.x + dx, anchor.y + dy };
                        basePlaced = true;
                        break;
                    }
                }
            }
        }
        const cc::IVec2 depotSpots[] = { { 2, 0 }, { 0, 2 }, { -1, 0 } };
        bool placed = false;
        for (const cc::IVec2 &spot : depotSpots)
        {
            if (trySite(BuildingType::ResourceDepot, { home.x + spot.x, home.y + spot.y }) !=
                kInvalidEntity)
            {
                placed = true;
                break;
            }
        }
        if (!placed)
        {
            trySpiral(BuildingType::ResourceDepot, home);
        }
        const cc::IVec2 factorySpots[] = { { 0, 2 }, { 3, 0 }, { -2, 2 } };
        placed = false;
        for (const cc::IVec2 &spot : factorySpots)
        {
            if (trySite(BuildingType::Factory, { home.x + spot.x, home.y + spot.y }) !=
                kInvalidEntity)
            {
                placed = true;
                break;
            }
        }
        if (!placed)
        {
            trySpiral(BuildingType::Factory, home);
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
    world.ai->Reset(difficulty, spots.aiHome, spots.playerHome, 1);
    world.ai->SetupBase();

    // 2v2: the allied commander plays team 0 beside the player, and a second
    // enemy holds the far side for team 1. Same difficulty all around; each
    // side's guards keep both teams fielded from frame one. Shared-team
    // economy: base income and harvest ticks are credited per team, so both
    // ledgers on a side benefit (allies genuinely share the pool).
    if (spots.is2v2)
    {
        if (world.allyAI != nullptr)
        {
            // SetupBase places the ally's own Base/Depot/Factory (no
            // placeBase: that would double-place on the same anchor).
            world.allyAI->Reset(difficulty, spots.allyHome, spots.aiHome, 0);
            world.allyAI->SetupBase();
        }
        if (world.enemyAI2 != nullptr)
        {
            world.enemyAI2->Reset(difficulty, spots.enemyHome2, spots.playerHome, 1);
            world.enemyAI2->SetupBase();
        }
    }

    *world.rallyPos = cc::ToRaylib(cc::TileToWorld(spots.playerHome.x + 4, spots.playerHome.y));
    world.camera->view.target = cc::ToRaylib(
        cc::TileToWorld(spots.playerHome.x, spots.playerHome.y) + cc::Vec2(32.0f, 32.0f));
    return true;
}
