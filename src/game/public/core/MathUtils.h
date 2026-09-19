#pragma once

#include <glm/glm.hpp>

#include "raylib.h"

// Central math header. All game-side math goes through glm types;
// raylib Vector2 conversions live here so (tile grid) and (units)
// don't scatter reinterpret casts across the codebase.
//
// NOTE: this file must NOT be named Math.h — on Windows' case-insensitive
// filesystem it would shadow the CRT <math.h> for every TU whose include
// path contains src/game/public, breaking <cmath>/<cstdlib> (MSVC C2039).

namespace cc
{

using Vec2 = glm::vec2;
using IVec2 = glm::ivec2;

// 64x64 tile grid (see Core Movement System).
inline constexpr float TILE_SIZE = 64.0f;

// --- raylib interop ---
Vec2 ToGlm(Vector2 v);
Vector2 ToRaylib(Vec2 v);

// --- tile grid helpers ---
// Tile indices -> world position of the tile's top-left corner.
Vec2 TileToWorld(int tileX, int tileY);
// World position -> containing tile indices.
IVec2 WorldToTile(Vec2 worldPos);
// Snap a world position to its tile's top-left corner.
Vec2 SnapToTile(Vec2 worldPos);

} // namespace cc
