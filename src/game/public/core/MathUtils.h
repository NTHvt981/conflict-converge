#pragma once

#include <glm/glm.hpp>

#include "raylib.h"

namespace cc
{

using Vec2 = glm::vec2;
using IVec2 = glm::ivec2;
using TileUnit = float;

inline constexpr float TILE_SIZE = 128.0f;
inline constexpr float kHalfTile = TILE_SIZE * 0.5f;
inline constexpr float kQuarterTile = TILE_SIZE * 0.25f;
inline constexpr float kRenderTileSize = 64.0f;
inline constexpr int kRenderTilesPerAxis = 2;
static_assert(kRenderTileSize * kRenderTilesPerAxis == TILE_SIZE,
			  "render grid must divide the logic tile");

Vec2 ToGlm(Vector2 v);
Vector2 ToRaylib(Vec2 v);

// World position of a point in tile space: a whole tile's top-left corner, or a
// fractional point inside it.
Vec2 TileToWorld(TileUnit x, TileUnit y);
IVec2 WorldToTile(Vec2 worldPos);
Vec2 SnapToTile(Vec2 worldPos);

} // namespace cc
