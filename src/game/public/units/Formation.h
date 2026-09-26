#pragma once

#include <cstddef>
#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "raylib.h"

class TileMap;
class OccupancyGrid;

namespace formation
{

std::vector<cc::IVec2> FormationOffsets(std::size_t count);

// `cellSize` is the max footprint dimension.
std::vector<cc::IVec2> FormationOffsetsFP(std::size_t count, int cellSize);

void IssueFormationMove(Registry &registry, const std::vector<Entity> &units, const TileMap &map,
                        Vector2 worldTarget);

// Selection order is ignored.
void IssueFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                          const TileMap &map, const OccupancyGrid &occ,
                          Vector2 worldTarget, bool slowestSpeed = false);

std::vector<cc::Vec2> LineFormationPositions(std::size_t count, cc::Vec2 lineStart,
                                             cc::Vec2 lineEnd);

void IssueLineFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                              const TileMap &map, const OccupancyGrid &occ,
                              Vector2 lineStartWorld, Vector2 lineEndWorld,
                              bool slowestSpeed = false);

} // namespace formation
