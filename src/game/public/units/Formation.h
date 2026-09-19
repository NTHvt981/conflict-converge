#pragma once

#include <cstddef>
#include <vector>

#include "MathUtils.h"
#include "Registry.h"
#include "raylib.h"

class TileMap;
class OccupancyGrid;

// Formation movement: a group ordered to a point fans out over neighbouring
// tiles so units don't stack; footprint-aware (2x2 vehicles get 2-tile spacing).

namespace formation
{

// Row-major tile offsets from the anchor for `count` units.
std::vector<cc::IVec2> FormationOffsets(std::size_t count);

// Footprint-aware offsets; `cellSize` is the max footprint dimension.
std::vector<cc::IVec2> FormationOffsetsFP(std::size_t count, int cellSize);

// Order each unit to its formation slot around the snapped target tile.
void IssueFormationMove(Registry &registry, const std::vector<Entity> &units, const TileMap &map,
                        Vector2 worldTarget);

// Footprint-aware variant using IssuePathOrderFootprint.
void IssueFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                          const TileMap &map, const OccupancyGrid &occ,
                          Vector2 worldTarget, bool slowestSpeed = false);

// Evenly spaced points along a segment (inclusive ends; midpoint for one unit).
std::vector<cc::Vec2> LineFormationPositions(std::size_t count, cc::Vec2 lineStart,
                                             cc::Vec2 lineEnd);

// Order units into a line between two world points.
void IssueLineFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                              const TileMap &map, const OccupancyGrid &occ,
                              Vector2 lineStartWorld, Vector2 lineEndWorld,
                              bool slowestSpeed = false);

} // namespace formation
