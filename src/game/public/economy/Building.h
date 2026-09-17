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
// through a small state machine: UnderConstruction -> Operational ->
// Destroyed. UnderConstruction is appended after Destroyed (not before
// Operational) so legacy saves keep decoding: wire values 0/1 are unchanged.

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
    UnderConstruction, // freshly placed: ramps health, gates in UpdateBuildingConstruction
    Count // keep last: save decode validates < Count
};

struct Building
{
    BuildingType type = BuildingType::Base;
    BuildingState state = BuildingState::Operational;
    int teamID = 0;
    // QoL building selection (mirrors Unit::isSelected): set by
    // SelectAllBuildings; rendered as a highlight ring. Not saved.
    bool isSelected = false;
    int tileX = 0; // top-left of the footprint
    int tileY = 0;
    // M13: structural HP so Engineers have something to repair. Nothing
    // damages buildings yet (combat is unit-vs-unit); tests wound them
    // directly until building attacks arrive. Defaults match Base (the
    // default type); PlaceBuilding stamps the real per-type max anyway.
    float health = 400.0f;
    float maxHealth = 400.0f;
    // Construction progress: seconds since placement while UnderConstruction
    // (not saved — a loaded site restarts its timer; see SaveGame.cpp).
    float constructionTime = 0.0f;
    // QoL auto-repair fractional accumulator (mirrors ResourceSystem's
    // income-carry idiom): sub-HP heal budget banks here across frames.
    float repairCarry = 0.0f;
};

// Full-health value per type (placement + save-load repair of legacy zeros).
float BuildingMaxHealth(BuildingType type);
// Seconds a fresh placement needs to reach Operational (Base 3, Depot 2,
// Factory 4 — a visible beat without stalling openings or flipping the
// soak ladder's tuned AI timings).
float BuildingBuildTime(BuildingType type);
// Advance every UnderConstruction building: ramp health proportionally,
// flip to Operational at exactly maxHealth on completion. UnderConstruction
// sites earn no income, can't produce, can't be repair/attack-targeted —
// every one of those gates already checks == Operational, so no gate
// changes are needed (verified, not assumed).
void UpdateBuildingConstruction(Registry &registry, float dt);

// Tile footprint (w, h) per building type.
cc::IVec2 Footprint(BuildingType type);

// World-space footprint rect (shared tile-to-rect math for the repair
// target scan, area repair, and previews — one implementation, not three
// inline copies).
Rectangle BuildingFootprintRect(const Building &building);

// Rect-vs-rect building query (mirrors QueryUnitsInRect's teamID
// convention: same team, or every team when negative). Footprints are
// rects, not points, so overlap — not containment — is the test.
void QueryBuildingsInRect(Registry &registry, Rectangle area, int teamID,
                          std::vector<Entity> &out);

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
// QoL area-build: the validation half of PlaceBuilding, exposed so the
// placement ghost can preview green/red without side effects.
bool CanPlaceBuilding(const TileMap &map, const ResourceNodes *nodes, BuildingType type,
                      int tileX, int tileY);
// QoL area-build: candidate footprint anchors tiling the inclusive tile
// range [minTile, maxTile], stepped by the type's footprint so placements
// never overlap each other. Callers attempt PlaceBuilding per slot and
// skip failures (blocked tiles simply don't build).
std::vector<cc::IVec2> AreaBuildSlots(BuildingType type, cc::IVec2 minTile,
                                      cc::IVec2 maxTile);

// Mark Destroyed, restore footprint tiles to Grass, remove the entity.
// Returns false for missing/non-building IDs (no-op).
bool DemolishBuilding(Registry &registry, TileMap &map, Entity entity);

// Base income generation (M5 Goal 4): every Operational Base trickles
// resources. teamID filters ownership (-1 = every team, test/demo default).
void UpdateBaseIncome(const Registry &registry, ResourceSystem &resources, float dt,
                      int teamID = -1);

// QoL building auto-repair: every Operational damaged building of teamID
// (or all teams if -1) heals toward maxHealth, funds-costed (iron per HP,
// atomic TrySpend quanta — pauses, never partially charges, when broke).
// Rate scales with capFraction (0 = paused, 1 = full rate); sub-HP budget
// banks in Building::repairCarry. Player-side v1 is a single global toggle
// on Game (no per-building selection exists yet); AI is deliberately not
// wired (its economy is soak-tuned, and Engineers already repair free).
void UpdateBuildingAutoRepair(Registry &registry, ResourceSystem &resources, float dt, int teamID,
                              float capFraction);

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
