#include "economy/Building.h"

#include <cmath>

#include "economy/BuildingTypeInfo.h"
#include "economy/Nodes.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"

inline constexpr float kBaseIronPerSecond = 2.0f;
inline constexpr float kBaseOilPerSecond = 1.0f;

cc::IVec2 Footprint(BuildingType type)
{
	return type == BuildingType::Count ? cc::IVec2{ 1, 1 } : InfoOf(type).footprint;
}

cc::IVec2 DefaultRallyTile(const TileMap &map, const Building &building)
{
	const cc::IVec2 fp = Footprint(building.type);
	const cc::IVec2 exit{ building.tileX + fp.x / 2, building.tileY + fp.y };
	return NearestFreeFootprintTile(map, nullptr, exit, 1, 1);
}

cc::IVec2 BuildingSpawnTile(const TileMap &map, const OccupancyGrid *occ,
							 const Building &building)
{
	const cc::IVec2 fp = Footprint(building.type);
	const cc::IVec2 exit{ building.tileX + fp.x / 2, building.tileY + fp.y };
	return NearestFreeFootprintTile(map, occ, exit, 1, 1);
}

Entity ProducerBuildingAt(Registry &registry, BuildingType type, TeamId teamID, int index)
{
	std::vector<Entity> producers;
	registry.Each<Building>([&](Entity id, const Building &building) {
		if (building.type == type && building.teamID == teamID &&
			building.state == BuildingState::Operational)
		{
			producers.push_back(id);
		}
	});
	if (producers.empty())
	{
		return kInvalidEntity;
	}
	return producers[static_cast<std::size_t>(index) %
					 producers.size()];
}

float BuildingMaxHealth(BuildingType type)
{
	return type == BuildingType::Count ? 200.0f : InfoOf(type).maxHealth;
}

Rectangle BuildingFootprintRect(const Building &building)
{
	const cc::IVec2 fp = Footprint(building.type);
	const cc::Vec2 corner = cc::TileToWorld(building.tileX, building.tileY);
	return { corner.x, corner.y, static_cast<float>(fp.x) * cc::TILE_SIZE,
			 static_cast<float>(fp.y) * cc::TILE_SIZE };
}

void QueryBuildingsInRect(Registry &registry, Rectangle area, TeamId teamID,
						  std::vector<Entity> &out)
{
	registry.Each<Building>([&](Entity id, const Building &building) {
		if (teamID != kAnyTeam && building.teamID != teamID)
		{
			return;
		}
		if (CheckCollisionRecs(BuildingFootprintRect(building), area))
		{
			out.push_back(id);
		}
	});
}

namespace
{

bool FootprintBuildable(const TileMap &map, cc::IVec2 size, int tileX, int tileY)
{
	for (int y = 0; y < size.y; ++y)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const cc::IVec2 tile{ tileX + x, tileY + y };
			if (!map.InBounds(tile) || map.Get(tile) != TerrainType::Grass)
			{
				return false;
			}
		}
	}
	return true;
}

void MarkFootprint(TileMap &map, cc::IVec2 size, int tileX, int tileY, TerrainType terrain)
{
	for (int y = 0; y < size.y; ++y)
	{
		for (int x = 0; x < size.x; ++x)
		{
			map.Set({ tileX + x, tileY + y }, terrain);
		}
	}
}

}

bool CanPlaceBuilding(const TileMap &map, const ResourceNodes *nodes, BuildingType type,
					  int tileX, int tileY)
{
	const cc::IVec2 size = Footprint(type);
	if (!FootprintBuildable(map, size, tileX, tileY))
	{
		return false;
	}
	if (nodes != nullptr)
	{
		for (int y = 0; y < size.y; ++y)
		{
			for (int x = 0; x < size.x; ++x)
			{
				if (nodes->FindAt({ tileX + x, tileY + y }) != nullptr)
				{
					return false;
				}
			}
		}
	}
	return true;
}

std::vector<cc::IVec2> AreaBuildSlots(BuildingType type, cc::IVec2 minTile,
									  cc::IVec2 maxTile)
{
	std::vector<cc::IVec2> slots;
	const cc::IVec2 fp = Footprint(type);
	if (fp.x <= 0 || fp.y <= 0)
	{
		return slots;
	}
	for (int y = minTile.y; y <= maxTile.y; y += fp.y)
	{
		for (int x = minTile.x; x <= maxTile.x; x += fp.x)
		{
			slots.push_back({ x, y });
		}
	}
	return slots;
}

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, TeamId teamID, int tileX,
					 int tileY)
{
	return PlaceBuilding(registry, map, type, teamID, tileX, tileY, nullptr);
}

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, TeamId teamID, int tileX,
					 int tileY, const ResourceNodes *nodes)
{
	if (!CanPlaceBuilding(map, nodes, type, tileX, tileY))
	{
		return kInvalidEntity;
	}
	const cc::IVec2 size = Footprint(type);
	Building building;
	building.type = type;
	building.state = BuildingState::UnderConstruction;
	building.teamID = teamID;
	building.tileX = tileX;
	building.tileY = tileY;
	building.health = 0.0f;
	building.maxHealth = BuildingMaxHealth(type);
	building.rallyTile = DefaultRallyTile(map, building);

	const Entity id = registry.Create();
	registry.Add(id, building);
	MarkFootprint(map, size, tileX, tileY, TerrainType::Building);
	return id;
}

bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity)
{
	Building *building = registry.Get<Building>(entity);
	if (building == nullptr)
	{
		return false;
	}
	building->state = BuildingState::Destroyed;
	MarkFootprint(map, Footprint(building->type), building->tileX, building->tileY,
				  TerrainType::Grass);
	registry.Destroy(entity);
	return true;
}

UnitCost BuildingCost(BuildingType type)
{
	if (type == BuildingType::Count)
	{
		return { 0, 0 };
	}
	const BuildingTypeInfo &info = InfoOf(type);
	return { info.ironCost, info.oilCost };
}

bool SellBuilding(Registry &registry, TileMap &map, ResourceSystem &resources, TeamId teamID,
				  Entity entity)
{
	const Building *building = registry.Get<Building>(entity);
	if (building == nullptr)
	{
		return false;
	}
	const UnitCost cost = BuildingCost(building->type);
	DepositIron(registry, resources, teamID, cost.iron / 2);
	DepositOil(registry, resources, teamID, cost.oil / 2);
	return DemolishBuilding(registry, map, entity);
}

void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt, TeamId teamID)
{
	int bases = 0;
	registry.Each<Building>([&](Entity, const Building &building) {
		if (building.state == BuildingState::Operational && building.type == BuildingType::Base &&
			(teamID == kAnyTeam || building.teamID == teamID))
		{
			++bases;
		}
	});
	if (bases > 0)
	{
		if (teamID == kAnyTeam)
		{
			resources.TickIncome(kBaseIronPerSecond * static_cast<float>(bases),
								 kBaseOilPerSecond * static_cast<float>(bases), dt);
		}
		else
		{
			const long cap = StorageCapacity(registry, teamID);
			resources.TickIncomeCapped(kBaseIronPerSecond * static_cast<float>(bases),
									   kBaseOilPerSecond * static_cast<float>(bases), dt, cap,
									   cap);
		}
	}
}

long StorageCapacity(const Registry &registry, TeamId teamID)
{
	long capacity = 500;
	registry.Each<Building>([&](Entity, const Building &building) {
		if (building.state == BuildingState::Operational &&
			building.type == BuildingType::ResourceDepot && building.teamID == teamID)
		{
			capacity += 1000;
		}
	});
	return capacity;
}

long DepositIron(const Registry &registry, ResourceSystem &resources, TeamId teamID, long amount)
{
	if (amount <= 0)
	{
		return 0;
	}
	long space = teamID == kAnyTeam ? amount : StorageCapacity(registry, teamID) - resources.iron;
	if (space <= 0)
	{
		return 0;
	}
	const long stored = amount < space ? amount : space;
	resources.AddIron(stored);
	return stored;
}

long DepositOil(const Registry &registry, ResourceSystem &resources, TeamId teamID, long amount)
{
	if (amount <= 0)
	{
		return 0;
	}
	long space = teamID == kAnyTeam ? amount : StorageCapacity(registry, teamID) - resources.oil;
	if (space <= 0)
	{
		return 0;
	}
	const long stored = amount < space ? amount : space;
	resources.AddOil(stored);
	return stored;
}

float BuildingBuildTime(BuildingType type)
{
	return type == BuildingType::Count ? 3.0f : InfoOf(type).buildTime;
}

std::optional<ProductionCategory> ProducerCategory(BuildingType type)
{
	if (type == BuildingType::Count)
	{
		return std::nullopt;
	}
	const BuildingTypeInfo &info = InfoOf(type);
	return info.isProducer ? std::optional<ProductionCategory>(info.producerCategory)
						   : std::nullopt;
}

void UpdateBuildingConstruction(Registry &registry, float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}
	registry.Each<Building>([&](Entity, Building &building) {
		if (building.state != BuildingState::UnderConstruction)
		{
			return;
		}
		building.constructionTime += dt;
		const float total = BuildingBuildTime(building.type);
		if (building.constructionTime >= total)
		{
			building.state = BuildingState::Operational;
			building.health = building.maxHealth;
			return;
		}
		building.health = building.maxHealth * (building.constructionTime / total);
	});
}

void UpdateBuildingFlash(Registry &registry, float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}
	registry.Each<Building>([&](Entity, Building &building) {
		if (building.hitFlashTime <= 0.0f)
		{
			return;
		}
		building.hitFlashTime -= dt;
		if (building.hitFlashTime < 0.0f)
		{
			building.hitFlashTime = 0.0f;
		}
	});
}

std::vector<cc::IVec2> BuildingEntrances(const TileMap &map, int tileX, int tileY,
										  int fpW, int fpH)
{
	std::vector<cc::IVec2> result;
	for (int y = tileY - 1; y <= tileY + fpH; ++y)
	{
		for (int x = tileX - 1; x <= tileX + fpW; ++x)
		{
			if (x >= tileX && x < tileX + fpW && y >= tileY && y < tileY + fpH)
			{
				continue;
			}
			if (!map.InBounds({ x, y }))
			{
				continue;
			}
			if (map.IsBlocked({ x, y }))
			{
				continue;
			}
			result.push_back({ x, y });
		}
	}
	return result;
}

std::vector<cc::IVec2> BuildingAttackPositions(const TileMap &map, int tileX, int tileY,
												int fpW, int fpH, int maxPositions)
{
	auto all = BuildingEntrances(map, tileX, tileY, fpW, fpH);
	if (all.empty() || static_cast<int>(all.size()) <= maxPositions)
	{
		return all;
	}
	std::vector<cc::IVec2> result;
	result.reserve(maxPositions);
	const float step = static_cast<float>(all.size()) / static_cast<float>(maxPositions);
	for (int i = 0; i < maxPositions; ++i)
	{
		const int idx = static_cast<int>(static_cast<float>(i) * step);
		result.push_back(all[idx]);
	}
	return result;
}
