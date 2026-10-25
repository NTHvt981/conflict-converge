#pragma once

#include <array>
#include <optional>
#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"
#include "economy/ResourceSystem.h"
#include "units/UnitFactory.h"
#include "units/UnitStats.h"

class TileMap;
class ResourceNodes;
class OccupancyGrid;

enum class BuildingType
{
	Base,
	ResourceDepot,
	Bootcamp,
	Workshop,
	Count
};

enum class BuildingState
{
	Operational,
	Destroyed,
	UnderConstruction,
	Count
};

struct Building
{
	BuildingType type = BuildingType::Base;
	BuildingState state = BuildingState::Operational;
	TeamId teamID = kPlayerTeam;
	bool isSelected = false;
	int tileX = 0; // top-left of the footprint
	int tileY = 0;
	cc::IVec2 rallyTile{ -1, -1 };
	float health = 400.0f;
	float maxHealth = 400.0f;
	float constructionTime = 0.0f;
	float hitFlashTime = 0.0f;
};

float BuildingMaxHealth(BuildingType type);
float BuildingBuildTime(BuildingType type);
UnitCost BuildingCost(BuildingType type);
std::optional<ProductionCategory> ProducerCategory(BuildingType type);
void UpdateBuildingConstruction(Registry &registry, float dt);
void UpdateBuildingFlash(Registry &registry, float dt);

cc::IVec2 Footprint(BuildingType type);
cc::IVec2 DefaultRallyTile(const TileMap &map, const Building &building);
cc::IVec2 BuildingSpawnTile(const TileMap &map, const OccupancyGrid *occ,
							 const Building &building);
Entity ProducerBuildingAt(Registry &registry, BuildingType type, TeamId teamID, int index);

Rectangle BuildingFootprintRect(const Building &building);

void QueryBuildingsInRect(Registry &registry, Rectangle area, TeamId teamID,
						  std::vector<Entity> &out);

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, TeamId teamID, int tileX,
					 int tileY);
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, TeamId teamID, int tileX,
					 int tileY, const ResourceNodes *nodes);
bool CanPlaceBuilding(const TileMap &map, const ResourceNodes *nodes, BuildingType type,
					  int tileX, int tileY);
std::vector<cc::IVec2> AreaBuildSlots(BuildingType type, cc::IVec2 minTile,
									  cc::IVec2 maxTile);

bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity);

// Sells for half cost through the storage cap, then demolishes.
bool SellBuilding(Registry &registry, TileMap &map, ResourceSystem &resources, TeamId teamID,
				  Entity entity);

void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt,
					   TeamId teamID = kAnyTeam);

// Storage cap: base allowance plus each operational depot; overflow deposits are lost.
long StorageCapacity(const Registry &registry, TeamId teamID);
long DepositIron(const Registry &registry, ResourceSystem &resources, TeamId teamID, long amount);
long DepositOil(const Registry &registry, ResourceSystem &resources, TeamId teamID, long amount);

std::vector<cc::IVec2> BuildingEntrances(const TileMap &map, int tileX, int tileY,
										  int fpW, int fpH);

std::vector<cc::IVec2> BuildingAttackPositions(const TileMap &map, int tileX, int tileY,
												int fpW, int fpH, int maxPositions = 12);
