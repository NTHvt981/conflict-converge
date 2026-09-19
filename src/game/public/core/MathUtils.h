#pragma once

#include <glm/glm.hpp>

#include "raylib.h"

// Central math header: game-side math uses glm types; raylib Vector2
// conversions live here.

namespace cc
{

using Vec2 = glm::vec2;
using IVec2 = glm::ivec2;

// 64x64 tile grid.
inline constexpr float TILE_SIZE = 64.0f;

Vec2 ToGlm(Vector2 v);
Vector2 ToRaylib(Vec2 v);

// Tile indices -> world position of the tile's top-left corner.
Vec2 TileToWorld(int tileX, int tileY);
// World position -> containing tile indices.
IVec2 WorldToTile(Vec2 worldPos);
// Snap a world position to its tile's top-left corner.
Vec2 SnapToTile(Vec2 worldPos);

} // namespace cc
