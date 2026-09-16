#pragma once

#include <cstddef>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Registry.h"  // Entity, Registry
#include "raylib.h"    // Vector2

class TileMap; // fwd-decl (Formation.cpp includes TileMap.h)
class OccupancyGrid; // fwd-decl: class, not struct (TileMap.h defines it as
                     // a class; struct here mangles a different symbol and
                     // breaks the link the moment a class-first TU calls in).

// M3 Goal 6: formation movement. A group ordered to a point fans out over
// neighboring tiles (row-major grid from the anchor) so units don't stack.
// Single-unit orders are unchanged: one offset, the anchor tile itself.
// Phase 7: footprint-aware — 2x2 vehicles get 2-tile spacing in the grid.

namespace formation
{

// Tile offsets from the anchor for `count` units: row-major over
// ceil(sqrt(count)) columns. Deterministic: slot i always maps to the same tile.
std::vector<cc::IVec2> FormationOffsets(std::size_t count);

// Phase 7: Footprint-aware formation offsets. `cellSize` is the maximum
// footprint dimension (max(fpW, fpH)) among the units. Offsets are
// multiplied by cellSize so 2x2 vehicles get 2-tile spacing.
std::vector<cc::IVec2> FormationOffsetsFP(std::size_t count, int cellSize);

// Order each unit to its formation slot around the snapped target tile
// (slot i -> anchor + offsets[i]), routed via IssuePathOrder.
void IssueFormationMove(Registry &registry, const std::vector<Entity> &units, const TileMap &map,
                        Vector2 worldTarget);

// Phase 7: Footprint-aware variant. Uses IssuePathOrderFootprint and
// offsets scaled by each unit's footprint so vehicles don't collide
// in formation. Reads fpW/fpH from each Unit component.
void IssueFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                          const TileMap &map, const OccupancyGrid &occ,
                          Vector2 worldTarget, bool slowestSpeed = false);

// QoL line formation: evenly spaces `count` points along the segment
// lineStart->lineEnd (inclusive ends when count > 1; the midpoint for a
// single unit). Pure geometry, unit-testable without a Registry (mirrors
// FormationOffsetsFP's shape).
std::vector<cc::Vec2> LineFormationPositions(std::size_t count, cc::Vec2 lineStart,
                                             cc::Vec2 lineEnd);

// Orders `units` into a line between lineStart/lineEnd (world space),
// each snapped to its nearest enterable tile exactly like
// IssueFormationMoveFP. Same per-unit dispatch, different slot source.
void IssueLineFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                              const TileMap &map, const OccupancyGrid &occ,
                              Vector2 lineStartWorld, Vector2 lineEndWorld,
                              bool slowestSpeed = false);

} // namespace formation
