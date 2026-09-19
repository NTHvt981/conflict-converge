#pragma once

#include <vector>

#include "MathUtils.h"
#include "TileMap.h"
#include "Unit.h"

// Tile-grid A* pathfinding: paths route around Water/Building/Rock and map
// edges (all blocked per TileMap::IsBlocked).

using TilePath = std::vector<cc::IVec2>;

// Per-terrain step cost (uniform 1.0; raise Forest for costly ground).
float TerrainCost(TerrainType terrain);

// 4-directional A* from start to goal (inclusive). Empty when unreachable,
// either end blocked, or the map has no tiles.
TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal);

// 8-directional footprint-aware A* using CanEnter for the full footprint.
TilePath FindPathFootprint(const TileMap &map, const OccupancyGrid &occ,
                           cc::IVec2 start, cc::IVec2 goal,
                           int footprintW, int footprintH,
                           Entity self, std::uint32_t selfGen);

// Nearest anchor tile the footprint can enter (ring search, radius 5).
cc::IVec2 NearestEnterableTile(const TileMap &map, const OccupancyGrid &occ,
                               cc::IVec2 want, int footprintW, int footprintH,
                               Entity self, std::uint32_t selfGen);

// Order a unit along an A* path to a world target; falls back to a straight
// move order when no path exists.
void IssuePathOrder(Unit &unit, const TileMap &map, Vector2 worldTarget);

// Footprint-aware path order (8-dir A* with CanEnter checks).
void IssuePathOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                             Vector2 worldTarget, Entity self, std::uint32_t selfGen);
