#pragma once

#include <vector>

#include "MathUtils.h" // cc::IVec2, tile helpers
#include "TileMap.h"   // blocked queries, OccupancyGrid
#include "Unit.h"      // Unit, Vector2

// Tile-grid A* pathfinding. orders walked straight lines and
// stopped at the first blocked tile; paths route around Water/Building and
// map edges (all blocked per TileMap::IsBlocked).

using TilePath = std::vector<cc::IVec2>;

// Per-terrain step cost. Uniform 1.0 for every passable tile;
// raise Forest here when designers want costly ground. Multipliers must stay
// >= 1.0: the Manhattan heuristic assumes unit minimum step cost.
float TerrainCost(TerrainType terrain);

// 4-directional A* from start to goal (inclusive both ends). Returns an
// empty path when unreachable, when either end is blocked, or when the map
// has no tiles. start == goal yields a single-node path.
TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal);

// 8-Directional footprint-aware A*. The search expands neighbors
// in all 8 directions with octile heuristic. Each candidate anchor is
// validated through OccupancyGrid::CanEnter for the unit's full footprint.
// Returns an empty path when no valid anchor position fits the footprint.
TilePath FindPathFootprint(const TileMap &map, const OccupancyGrid &occ,
                           cc::IVec2 start, cc::IVec2 goal,
                           int footprintW, int footprintH,
                           Entity self, std::uint32_t selfGen);

// Nearest anchor tile the footprint can enter (ring search, radius 5).
// Returns the request unchanged when it is already enterable, the nearest
// enterable tile otherwise, or the request when nothing enterable is near.
// Pure, headless-safe. Used to sanitize order goals clicked onto units.
cc::IVec2 NearestEnterableTile(const TileMap &map, const OccupancyGrid &occ,
                               cc::IVec2 want, int footprintW, int footprintH,
                               Entity self, std::uint32_t selfGen);

// Order a unit along an A* path to a world target (target snapped to its
// tile). Clears any previous path. Falls back to a straight move order
// when no path exists, so the unit still reacts to the click.
void IssuePathOrder(Unit &unit, const TileMap &map, Vector2 worldTarget);

// Footprint-aware path order. Uses 8-dir A* with CanEnter checks.
void IssuePathOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                             Vector2 worldTarget, Entity self, std::uint32_t selfGen);
