#pragma once

#include <cstddef>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Registry.h"  // Entity, Registry
#include "raylib.h"    // Vector2

class TileMap; // fwd-decl (Formation.cpp includes TileMap.h)

// M3 Goal 6: formation movement. A group ordered to a point fans out over
// neighboring tiles (row-major grid from the anchor) so units don't stack.
// Single-unit orders are unchanged: one offset, the anchor tile itself.

namespace formation
{

// Tile offsets from the anchor for `count` units: row-major over
// ceil(sqrt(count)) columns. Deterministic: slot i always maps to the same tile.
std::vector<cc::IVec2> FormationOffsets(std::size_t count);

// Order each unit to its formation slot around the snapped target tile
// (slot i -> anchor + offsets[i]), routed via IssuePathOrder.
void IssueFormationMove(Registry &registry, const std::vector<Entity> &units, const TileMap &map,
                        Vector2 worldTarget);

} // namespace formation
