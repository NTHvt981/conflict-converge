#pragma once

#include <array>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Registry.h"  // Entity, Registry
#include "ResourceSystem.h" // UpdateBaseIncome destination

class TileMap; // fwd-decl (Building.cpp includes TileMap.h)
class ResourceNodes; // fwd-decl (node-occupancy guard, Building.cpp includes Nodes.h)

// M5 Goals 2/5: base building mechanics. Buildings occupy a tile footprint
// (marked TerrainType::Building, hence blocked + overlap-proof) and go
// through a small state machine: Operational -> Destroyed.

enum class BuildingType
{
    Base,         // 2x2, generates base income (see UpdateBaseIncome)
    ResourceDepot, // 1x1, drop-off marker for future harvester routes
    Factory,       // 2x2, owns a unit production queue (M5 Goal 6)
    Count // keep last: save decode validates < Count
};

enum class BuildingState
{
    Operational,
    Destroyed, // terminal; entity removal follows via DemolishBuilding
    Count // keep last: save decode validates < Count
};

struct Building
{
    BuildingType type = BuildingType::Base;
    BuildingState state = BuildingState::Operational;
    int teamID = 0;
    int tileX = 0; // top-left of the footprint
    int tileY = 0;
    // M13: structural HP so Engineers have something to repair. Nothing
    // damages buildings yet (combat is unit-vs-unit); tests wound them
    // directly until building attacks arrive.
    float health = 600.0f;
    float maxHealth = 600.0f;
};

// Full-health value per type (placement + save-load repair of legacy zeros).
float BuildingMaxHealth(BuildingType type);

// Tile footprint (w, h) per building type.
cc::IVec2 Footprint(BuildingType type);

// Validate (in-bounds, all Grass, team stored) and place: creates the entity,
// attaches the component, marks footprint tiles as Building.
// Returns kInvalidEntity when any footprint tile is out-of-bounds or not Grass.
// The nodes-aware overload additionally refuses footprints covering a live
// (or depleted-but-respawning) resource node — node tiles stay Grass by
// design so gatherers can stand on them, and building over one would strand
// the resource permanently. Pass nullptr to keep the legacy behavior.
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY);
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY, const ResourceNodes *nodes);

// Mark Destroyed, restore footprint tiles to Grass, remove the entity.
// Returns false for missing/non-building IDs (no-op).
bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity);

// Base income generation (M5 Goal 4): every Operational Base trickles
// resources. teamID filters ownership (-1 = every team, test/demo default).
void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt,
                      int teamID = -1);

// Phase 5: Walkable entrance tiles adjacent to a building's footprint.
// Returns tiles outside the footprint that are in-bounds and not blocked
// (grass/forest). Suitable for units to stand on when entering/repairing.
std::vector<cc::IVec2> BuildingEntrances(const TileMap &map, int tileX, int tileY,
                                          int fpW, int fpH);

// Phase 5: Attack positions around a building. Returns up to maxPositions
// perimeter tiles distributed around the footprint. Units are spread
// evenly across available positions so they don't all converge on one tile.
std::vector<cc::IVec2> BuildingAttackPositions(const TileMap &map, int tileX, int tileY,
                                                int fpW, int fpH, int maxPositions = 12);
