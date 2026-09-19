#include "Skirmish.h"

#include "Building.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "MapFile.h"
#include "Nodes.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitFactory.h"

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
		const cc::IVec2 free = NearestFreeTile(map, tileX, tileY);
		factory.Spawn(type, team, cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
	};
	auto placeBase = [&](int team, cc::IVec2 anchor) {
		auto reserve = [&](cc::IVec2 at, BuildingType type, Entity entity) {
			const cc::IVec2 fp = Footprint(type);
			occ.ReserveFootprint(at, fp.x, fp.y, entity, registry.Generation(entity));
		};
		auto trySite = [&](BuildingType type, cc::IVec2 at) -> Entity {
			const Entity e =
				PlaceBuilding(registry, map, type, team, at.x, at.y, &nodes);
			if (e != kInvalidEntity)
			{
				reserve(at, type, e);
			}
			return e;
		};
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
	placeBase(0, spots.playerHome);
	spawnDemo(UnitType::Infantry, spots.playerHome.x, spots.playerHome.y, 0);
	spawnDemo(UnitType::LightTank, spots.playerHome.x + 2, spots.playerHome.y, 0);
	spawnDemo(UnitType::Artillery, spots.playerHome.x + 1, spots.playerHome.y + 3, 0);
	spawnDemo(UnitType::Engineer, spots.harvest.x, spots.harvest.y, 0);
	queue.Enqueue(resources, UnitType::Infantry);
	queue.Enqueue(resources, UnitType::LightTank);

	world.ai->Reset(difficulty, spots.aiHome, spots.playerHome, 1);
	world.ai->SetupBase();

	if (spots.is2v2)
	{
		if (world.allyAI != nullptr)
		{
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

bool BuildSandbox(SkirmishWorld &world, const std::string &mapPath)
{
	if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
		world.occ == nullptr || world.fog == nullptr || world.nodes == nullptr ||
		world.queue == nullptr || world.factory == nullptr || world.camera == nullptr ||
		world.rallyPos == nullptr)
	{
		return false;
	}
	ResetSkirmish(world);

	Registry &registry = *world.registry;
	TileMap &map = *world.map;
	OccupancyGrid &occ = *world.occ;
	FogOfWar &fog = *world.fog;
	ResourceNodes &nodes = *world.nodes;
	UnitFactory &factory = *world.factory;

	MapData data;
	if (!ParseMapFile(mapPath, data) || data.playerSpawns.empty())
	{
		return false;
	}
	ApplyMapData(data, map, nodes, &occ);
	fog.Resize(map.Width(), map.Height());
	occ.Resize(map.Width(), map.Height());

	const cc::IVec2 home = data.playerSpawns[0];
	const cc::IVec2 free = NearestFreeTile(map, home.x, home.y);
	factory.SpawnPrepaid(UnitType::PrototypeInfantry, 0,
		cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
	factory.SpawnPrepaid(UnitType::PrototypeInfantry, 0,
		cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
	factory.SpawnPrepaid(UnitType::PrototypeInfantry, 0,
		cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
	factory.SpawnPrepaid(UnitType::PrototypeInfantry, 0,
		cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
	factory.SpawnPrepaid(UnitType::PrototypeInfantry, 0,
		cc::ToRaylib(cc::TileToWorld(free.x, free.y)));
	factory.SpawnPrepaid(UnitType::PrototypeInfantry, 0,
		cc::ToRaylib(cc::TileToWorld(free.x, free.y)));

	*world.rallyPos = cc::ToRaylib(cc::TileToWorld(free.x, free.y));
	world.camera->view.target = cc::ToRaylib(cc::TileToWorld(free.x, free.y) +
											 cc::Vec2(32.0f, 32.0f));
	return true;
}
