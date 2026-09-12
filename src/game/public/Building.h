#pragma once

#include "MathUtils.h" // cc::IVec2
#include "Registry.h"  // Entity, Registry
#include "ResourceSystem.h" // UpdateBaseIncome destination

class TileMap; // fwd-decl (Building.cpp includes TileMap.h)

// M5 Goals 2/5: base building mechanics. Buildings occupy a tile footprint
// (marked TerrainType::Building, hence blocked + overlap-proof) and go
// through a small state machine: Operational -> Destroyed.

enum class BuildingType
{
    Base,         // 2x2, generates base income (see UpdateBaseIncome)
    ResourceDepot, // 1x1, drop-off marker for future harvester routes
    Factory       // 2x2, owns a unit production queue (M5 Goal 6)
};

enum class BuildingState
{
    Operational,
    Destroyed // terminal; entity removal follows via DemolishBuilding
};

struct Building
{
    BuildingType type = BuildingType::Base;
    BuildingState state = BuildingState::Operational;
    int teamID = 0;
    int tileX = 0; // top-left of the footprint
    int tileY = 0;
};

// Tile footprint (w, h) per building type.
cc::IVec2 Footprint(BuildingType type);

// Validate (in-bounds, all Grass, team stored) and place: creates the entity,
// attaches the component, marks footprint tiles as Building.
// Returns kInvalidEntity when any footprint tile is out-of-bounds or not Grass.
Entity PlaceBuilding(Registry &registry, TileMap &map, BuildingType type, int teamID, int tileX,
                     int tileY);

// Mark Destroyed, restore footprint tiles to Grass, remove the entity.
// Returns false for missing/non-building IDs (no-op).
bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity);

// Base income generation (M5 Goal 4): every Operational Base trickles
// resources. teamID filters ownership (-1 = every team, test/demo default).
void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt,
                      int teamID = -1);
