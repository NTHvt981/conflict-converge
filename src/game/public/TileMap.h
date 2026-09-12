#pragma once

#include <cstdint>
#include <vector>

#include "CcAssert.h"
#include "MathUtils.h" // cc::IVec2, cc::TILE_SIZE

// M2 Goal 1: tile-based movement system. TileMap owns the 64x64 grid:
// dimensions, per-tile terrain, and blocked queries for movement (M2),
// pathfinding (M3), and building placement (M5). World/tile conversion
// lives in MathUtils.h; this class only stores and answers about tiles.

enum class TerrainType : std::uint8_t
{
    Grass,    // passable, default fill
    Water,    // blocked (impassable)
    Building  // blocked (occupied by a structure)
};

class TileMap
{
public:
    TileMap(int widthTiles, int heightTiles);

    int Width() const;
    int Height() const;

    bool InBounds(cc::IVec2 tile) const;

    // Get asserts InBounds; use InBounds/IsBlocked for unchecked queries.
    TerrainType Get(cc::IVec2 tile) const;
    void Set(cc::IVec2 tile, TerrainType terrain);

    // Blocked = Water or Building. Out-of-bounds counts as blocked so
    // movement stays inside the map without extra edge checks.
    bool IsBlocked(cc::IVec2 tile) const;

    void Clear(TerrainType fill = TerrainType::Grass);

private:
    int Index(cc::IVec2 tile) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<TerrainType> tiles_;
};
