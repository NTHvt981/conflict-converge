// Unit tests for math helpers (cc:: tile grid + raylib interop).

#include "test_harness.h"

#include "core/MathUtils.h"

static_assert(sizeof(cc::Vec2) == 8, "cc::Vec2 must be two floats");
static_assert(cc::TILE_SIZE == 128.0f, "tile grid must be 128x128");

void RunMathUtilsTests()
{
	Vector2 ray = { 12.5f, -3.25f };
	CC_CHECK(cc::ToRaylib(cc::ToGlm(ray)).x == ray.x);
	CC_CHECK(cc::ToRaylib(cc::ToGlm(ray)).y == ray.y);
	CC_CHECK(cc::ToGlm(cc::ToRaylib(cc::Vec2(7.0f, 9.0f))) == cc::Vec2(7.0f, 9.0f));

	CC_CHECK(cc::TileToWorld(0, 0) == cc::Vec2(0.0f, 0.0f));
	CC_CHECK(cc::TileToWorld(2, 3) == cc::Vec2(256.0f, 384.0f));
	// Fractional tile coordinates scale linearly; negative tiles floor, not truncate.
	CC_CHECK(cc::TileToWorld(2.5f, 3.25f) == cc::Vec2(320.0f, 416.0f));
	CC_CHECK(cc::WorldToTile(cc::TileToWorld(2.5f, 3.25f)) == cc::IVec2(2, 3));
	CC_CHECK(cc::WorldToTile(cc::TileToWorld(-0.5f, -2.5f)) == cc::IVec2(-1, -3));
	CC_CHECK(cc::WorldToTile(cc::Vec2(140.0f, 260.0f)) == cc::IVec2(1, 2));
	CC_CHECK(cc::SnapToTile(cc::Vec2(140.0f, 260.0f)) == cc::Vec2(128.0f, 256.0f));

	// Tile origin snaps to itself; exact corner belongs to its own tile.
	CC_CHECK(cc::SnapToTile(cc::Vec2(128.0f, 256.0f)) == cc::Vec2(128.0f, 256.0f));
	CC_CHECK(cc::WorldToTile(cc::Vec2(128.0f, 256.0f)) == cc::IVec2(1, 2));

	for (int x = 0; x < 4; ++x)
	{
		for (int y = 0; y < 4; ++y)
		{
			CC_CHECK(cc::WorldToTile(cc::TileToWorld(x, y)) == cc::IVec2(x, y));
		}
	}
	cc::Vec2 snapped = cc::SnapToTile(cc::Vec2(400.0f, 20.0f));
	CC_CHECK(snapped == cc::Vec2(384.0f, 0.0f));
	CC_CHECK(cc::WorldToTile(snapped) == cc::IVec2(3, 0));
}
