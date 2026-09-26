// Unit tests for math helpers (cc:: tile grid + raylib interop).
// Tile convention: indices -> world position of the tile's top-left corner.

#include "test_harness.h"

#include "core/MathUtils.h"

static_assert(sizeof(cc::Vec2) == 8, "cc::Vec2 must be two floats");
static_assert(cc::TILE_SIZE == 64.0f, "tile grid must be 64x64");

void RunMathUtilsTests()
{
    // --- raylib interop round-trip ---
    Vector2 ray = { 12.5f, -3.25f };
    CC_CHECK(cc::ToRaylib(cc::ToGlm(ray)).x == ray.x);
    CC_CHECK(cc::ToRaylib(cc::ToGlm(ray)).y == ray.y);
    CC_CHECK(cc::ToGlm(cc::ToRaylib(cc::Vec2(7.0f, 9.0f))) == cc::Vec2(7.0f, 9.0f));

    // --- tile grid: known mappings ---
    CC_CHECK(cc::TileToWorld(0, 0) == cc::Vec2(0.0f, 0.0f));
    CC_CHECK(cc::TileToWorld(2, 3) == cc::Vec2(128.0f, 192.0f));
    CC_CHECK(cc::WorldToTile(cc::Vec2(70.0f, 130.0f)) == cc::IVec2(1, 2));
    CC_CHECK(cc::SnapToTile(cc::Vec2(70.0f, 130.0f)) == cc::Vec2(64.0f, 128.0f));

    // Tile origin snaps to itself; exact corner belongs to its own tile.
    CC_CHECK(cc::SnapToTile(cc::Vec2(64.0f, 128.0f)) == cc::Vec2(64.0f, 128.0f));
    CC_CHECK(cc::WorldToTile(cc::Vec2(64.0f, 128.0f)) == cc::IVec2(1, 2));

    // --- round-trips: tile -> world -> tile, world -> tile -> snapped world ---
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            CC_CHECK(cc::WorldToTile(cc::TileToWorld(x, y)) == cc::IVec2(x, y));
        }
    }
    cc::Vec2 snapped = cc::SnapToTile(cc::Vec2(200.0f, 10.0f));
    CC_CHECK(snapped == cc::Vec2(192.0f, 0.0f));
    CC_CHECK(cc::WorldToTile(snapped) == cc::IVec2(3, 0));
}
