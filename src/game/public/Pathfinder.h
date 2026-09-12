#pragma once

#include <vector>

#include "MathUtils.h" // cc::IVec2, tile helpers
#include "TileMap.h"   // blocked queries
#include "Unit.h"      // Unit, Vector2

// M3 Goal 3: tile-grid A* pathfinding. M2 orders walked straight lines and
// stopped at the first blocked tile; paths route around Water/Building and
// map edges (all blocked per TileMap::IsBlocked).

using TilePath = std::vector<cc::IVec2>;

// 4-directional A* from start to goal (inclusive both ends). Returns an
// empty path when unreachable, when either end is blocked, or when the map
// has no tiles. start == goal yields a single-node path.
TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal);

// Order a unit along an A* path to a world target (target snapped to its
// tile). Clears any previous path. Falls back to a straight M2 move order
// when no path exists, so the unit still reacts to the click.
void IssuePathOrder(Unit &unit, const TileMap &map, Vector2 worldTarget);
