#pragma once

#include <glm/glm.hpp>

#include "raylib.h"

namespace cc
{

using Vec2 = glm::vec2;
using IVec2 = glm::ivec2;

inline constexpr float TILE_SIZE = 64.0f;

Vec2 ToGlm(Vector2 v);
Vector2 ToRaylib(Vec2 v);

// Tile indices -> world position of the tile's top-left corner.
Vec2 TileToWorld(int tileX, int tileY);
IVec2 WorldToTile(Vec2 worldPos);
Vec2 SnapToTile(Vec2 worldPos);

} // namespace cc
