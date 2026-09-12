// Unit tests for the M2 Goal 1 TileMap (terrain storage, bounds, blocking).

#include "test_harness.h"

#include "TileMap.h"

void RunTileMapTests()
{
    TileMap map(10, 8);
    CC_CHECK(map.Width() == 10);
    CC_CHECK(map.Height() == 8);

    // --- default fill is passable grass ---
    CC_CHECK(map.Get({ 0, 0 }) == TerrainType::Grass);
    CC_CHECK(map.Get({ 9, 7 }) == TerrainType::Grass);
    CC_CHECK(!map.IsBlocked({ 0, 0 }));

    // --- bounds ---
    CC_CHECK(map.InBounds({ 0, 0 }));
    CC_CHECK(map.InBounds({ 9, 7 }));
    CC_CHECK(!map.InBounds({ 10, 0 }));
    CC_CHECK(!map.InBounds({ 0, 8 }));
    CC_CHECK(!map.InBounds({ -1, 0 }));
    CC_CHECK(!map.InBounds({ 0, -1 }));

    // --- terrain set/get + blocking rules ---
    map.Set({ 3, 4 }, TerrainType::Water);
    map.Set({ 5, 6 }, TerrainType::Building);
    CC_CHECK(map.Get({ 3, 4 }) == TerrainType::Water);
    CC_CHECK(map.Get({ 5, 6 }) == TerrainType::Building);
    CC_CHECK(map.IsBlocked({ 3, 4 }));
    CC_CHECK(map.IsBlocked({ 5, 6 }));
    CC_CHECK(!map.IsBlocked({ 3, 5 }));

    // --- out-of-bounds counts as blocked (keeps units inside) ---
    CC_CHECK(map.IsBlocked({ 10, 0 }));
    CC_CHECK(map.IsBlocked({ -1, -1 }));

    // --- clear resets the whole grid ---
    map.Clear();
    CC_CHECK(map.Get({ 3, 4 }) == TerrainType::Grass);
    CC_CHECK(!map.IsBlocked({ 5, 6 }));
    map.Clear(TerrainType::Water);
    CC_CHECK(map.IsBlocked({ 0, 0 }));
    CC_CHECK(map.IsBlocked({ 9, 7 }));
}
