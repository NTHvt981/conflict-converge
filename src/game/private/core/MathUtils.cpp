#include "core/MathUtils.h"

#include <glm/gtc/round.hpp>

namespace cc
{

Vec2 ToGlm(Vector2 v)
{
	return Vec2(v.x, v.y);
}

Vector2 ToRaylib(Vec2 v)
{
	return Vector2{v.x, v.y};
}

Vec2 TileToWorld(TileUnit x, TileUnit y)
{
	return Vec2(x * TILE_SIZE, y * TILE_SIZE);
}

IVec2 WorldToTile(Vec2 worldPos)
{
	const Vec2 t = glm::floor(worldPos / TILE_SIZE);
	return IVec2(static_cast<int>(t.x), static_cast<int>(t.y));
}

Vec2 SnapToTile(Vec2 worldPos)
{
	const IVec2 t = WorldToTile(worldPos);
	return TileToWorld(t.x, t.y);
}

} // namespace cc
