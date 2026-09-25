#pragma once

#include <array>
#include <vector>

#include "MathUtils.h"
#include "Registry.h"
#include "ResourceSystem.h"

class TileMap;
class ResourceNodes;

// Base building mechanics. Buildings occupy a tile footprint (marked
// TerrainType::Building, hence blocked + overlap-proof) and go through a
// small state machine: UnderConstruction -> Operational -> Destroyed.

enum class BuildingType
{
    Base,         // 2x2, generates base income (see UpdateBaseIncome)
    ResourceDepot, // 1x1, drop-off marker for future harvester routes
    Factory,       // 2x2, owns a unit production queue
    Count
};

enum class BuildingState
{
    Operational,
    Destroyed, // terminal; entity removal follows via DemolishBuilding
    UnderConstruction, // freshly placed: ramps health, gates in UpdateBuildingConstruction
    Count
};

struct Building
{
    BuildingType type = BuildingType::Base;
    BuildingState state = BuildingState::Operational;
    int teamID = 0;
    // QoL building selection; rendered as a highlight ring. Not saved.
    bool isSelected = false;
    int tileX = 0; // top-left of the footprint
    int tileY = 0;
    // Structural HP so Engineers have something to repair.
    float health = 400.0f;
    float maxHealth = 400.0f;
    // Seconds since placement while UnderConstruction (not saved).
    float constructionTime = 0.0f;
    // QoL auto-repair fractional accumulator (mirrors ResourceSystem carry).
    float repairCarry = 0.0f;
};

// Full-health value per type (placement + save-load repair of zero values).
float BuildingMaxHealth(BuildingType type);
// Seconds a fresh placement needs to reach Operational.
float BuildingBuildTime(BuildingType type);
// Advance every UnderConstruction building: ramp health, flip to Operational.
void UpdateBuildingConstruction(Registry &registry, float dt);

// Tile footprint (w, h) per building type.
cc::IVec2 Footprint(BuildingType type);

// World-space footprint rect (shared by repair scan, area repair, previews).
Rectangle BuildingFootprintRect(const Building &building);

// Rect-vs-rect building query (teamID < 0 = every team; overlap, not containment).
void QueryBuildingsInRect(Registry &registry, Rectangle area, int teamID,
                          std::vector<Entity> &out);

// Validate (in-bounds, all Grass, team stored) and place, marking footprint
// tiles as Building. Returns kInvalidEntity on invalid tiles.
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY);
// Nodes-aware overload: also refuses footprints covering a live resource node.
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY, const ResourceNodes *nodes);
// QoL area-build: the validation half of PlaceBuilding, side-effect free.
bool CanPlaceBuilding(const TileMap &map, const ResourceNodes *nodes, BuildingType type,
                      int tileX, int tileY);
// Candidate footprint anchors tiling [minTile, maxTile], stepped by the
// footprint so placements never overlap each other.
std::vector<cc::IVec2> AreaBuildSlots(BuildingType type, cc::IVec2 minTile,
                                      cc::IVec2 maxTile);

// Mark Destroyed, restore footprint tiles to Grass, remove the entity.
bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity);

// Base income: every Operational Base trickles resources (teamID < 0 = all).
void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt,
                      int teamID = -1);

// QoL auto-repair: Operational damaged buildings heal toward maxHealth,
// funds-costed (iron per HP, atomic quanta); capFraction 0 = paused.
void UpdateBuildingAutoRepair(Registry &registry, ResourceSystem &resources, float dt, int teamID,
                              float capFraction);

// Walkable entrance tiles adjacent to a building's footprint.
std::vector<cc::IVec2> BuildingEntrances(const TileMap &map, int tileX, int tileY,
                                          int fpW, int fpH);

// Up to maxPositions perimeter tiles around a building for attackers to spread over.
std::vector<cc::IVec2> BuildingAttackPositions(const TileMap &map, int tileX, int tileY,
                                                int fpW, int fpH, int maxPositions = 12);
