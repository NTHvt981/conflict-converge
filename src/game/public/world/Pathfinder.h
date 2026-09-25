#pragma once

#include <vector>

#include "MathUtils.h"
#include "TileMap.h"
#include "Unit.h"

using TilePath = std::vector<cc::IVec2>;

float TerrainCost(TerrainType terrain);

// 4-directional A* from start to goal (inclusive). Empty when unreachable,
// either end blocked, or the map has no tiles.
TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal);

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
void IssuePathOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                    Vector2 worldTarget);

void IssuePathOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                             const OccupancyGrid &occ, Vector2 worldTarget, Entity self,
                             std::uint32_t selfGen);
