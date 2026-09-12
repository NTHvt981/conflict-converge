#include "TileMap.h"

#include <algorithm>

TileMap::TileMap(int widthTiles, int heightTiles)
    : width_(widthTiles)
    , height_(heightTiles)
    , tiles_(static_cast<std::size_t>(widthTiles) * static_cast<std::size_t>(heightTiles),
             TerrainType::Grass)
{
    CC_ASSERT(widthTiles > 0 && heightTiles > 0);
}

int TileMap::Width() const
{
    return width_;
}

int TileMap::Height() const
{
    return height_;
}

bool TileMap::InBounds(cc::IVec2 tile) const
{
    return tile.x >= 0 && tile.y >= 0 && tile.x < width_ && tile.y < height_;
}

TerrainType TileMap::Get(cc::IVec2 tile) const
{
    CC_ASSERT(InBounds(tile));
    return tiles_[Index(tile)];
}

void TileMap::Set(cc::IVec2 tile, TerrainType terrain)
{
    CC_ASSERT(InBounds(tile));
    tiles_[Index(tile)] = terrain;
}

bool TileMap::IsBlocked(cc::IVec2 tile) const
{
    if (!InBounds(tile))
    {
        return true;
    }
    TerrainType terrain = tiles_[Index(tile)];
    return terrain == TerrainType::Water || terrain == TerrainType::Building ||
           terrain == TerrainType::Rock;
}

void TileMap::Clear(TerrainType fill)
{
    std::fill(tiles_.begin(), tiles_.end(), fill);
}

void TileMap::Resize(int widthTiles, int heightTiles)
{
    CC_ASSERT(widthTiles > 0 && heightTiles > 0);
    width_ = widthTiles;
    height_ = heightTiles;
    tiles_.assign(static_cast<std::size_t>(widthTiles) * static_cast<std::size_t>(heightTiles),
                  TerrainType::Grass);
}

int TileMap::Index(cc::IVec2 tile) const
{
    return tile.y * width_ + tile.x;
}
