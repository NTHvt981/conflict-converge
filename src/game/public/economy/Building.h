#pragma once

#include <array>
#include <optional>
#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "units/UnitStats.h"

class TileMap;
class ResourceNodes;

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
    int teamID = 0;
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
std::optional<ProductionCategory> ProducerCategory(BuildingType type);
void UpdateBuildingConstruction(Registry &registry, float dt);
void UpdateBuildingFlash(Registry &registry, float dt);

cc::IVec2 Footprint(BuildingType type);
cc::IVec2 DefaultRallyTile(const TileMap &map, const Building &building);

Rectangle BuildingFootprintRect(const Building &building);

// teamID < 0 = every team; overlap, not containment.
void QueryBuildingsInRect(Registry &registry, Rectangle area, int teamID,
                          std::vector<Entity> &out);

Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY);
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY, const ResourceNodes *nodes);
bool CanPlaceBuilding(const TileMap &map, const ResourceNodes *nodes, BuildingType type,
                      int tileX, int tileY);
std::vector<cc::IVec2> AreaBuildSlots(BuildingType type, cc::IVec2 minTile,
                                      cc::IVec2 maxTile);

bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity);

void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt,
                      int teamID = -1);

std::vector<cc::IVec2> BuildingEntrances(const TileMap &map, int tileX, int tileY,
                                          int fpW, int fpH);

std::vector<cc::IVec2> BuildingAttackPositions(const TileMap &map, int tileX, int tileY,
                                                int fpW, int fpH, int maxPositions = 12);
