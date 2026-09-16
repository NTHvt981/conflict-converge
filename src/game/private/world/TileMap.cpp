#include "TileMap.h"

#include <algorithm>
#include <cstdint>

namespace
{
// Same budget as MapFile's kMaxTiles: guards the size_t cast below so a
// negative or gigantic dimension yields an empty grid instead of a
// multi-exabyte vector allocation that throws before any assert could run.
constexpr std::int64_t kMaxGridTiles = 1024 * 1024;

bool GridDimsValid(int widthTiles, int heightTiles, std::size_t &outCount)
{
    if (widthTiles <= 0 || heightTiles <= 0)
    {
        return false;
    }
    const std::int64_t count =
        static_cast<std::int64_t>(widthTiles) * static_cast<std::int64_t>(heightTiles);
    if (count > kMaxGridTiles)
    {
        return false;
    }
    outCount = static_cast<std::size_t>(count);
    return true;
}
} // namespace

TileMap::TileMap(int widthTiles, int heightTiles)
    : width_(0)
    , height_(0)
    , tiles_()
{
    CC_ASSERT(widthTiles > 0 && heightTiles > 0);
    std::size_t count = 0;
    if (!GridDimsValid(widthTiles, heightTiles, count))
    {
        return; // degenerate 0x0 grid: InBounds false, IsBlocked true
    }
    width_ = widthTiles;
    height_ = heightTiles;
    tiles_.assign(count, TerrainType::Grass);
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
    std::size_t count = 0;
    if (!GridDimsValid(widthTiles, heightTiles, count))
    {
        return; // keep existing contents rather than corrupt on bad input
    }
    width_ = widthTiles;
    height_ = heightTiles;
    tiles_.assign(count, TerrainType::Grass);
}

int TileMap::Index(cc::IVec2 tile) const
{
    return tile.y * width_ + tile.x;
}

// --- OccupancyGrid ---

OccupancyGrid::OccupancyGrid(int widthTiles, int heightTiles)
    : width_(0)
    , height_(0)
    , units_()
    , buildings_()
{
    CC_ASSERT(widthTiles > 0 && heightTiles > 0);
    std::size_t count = 0;
    if (!GridDimsValid(widthTiles, heightTiles, count))
    {
        return; // degenerate 0x0 grid: InBounds false, CanEnter false
    }
    width_ = widthTiles;
    height_ = heightTiles;
    units_.assign(count, OccEntry{});
    buildings_.assign(count, kOccEmpty);
}

int OccupancyGrid::Width() const
{
    return width_;
}

int OccupancyGrid::Height() const
{
    return height_;
}

bool OccupancyGrid::InBounds(cc::IVec2 tile) const
{
    return tile.x >= 0 && tile.y >= 0 && tile.x < width_ && tile.y < height_;
}

OccEntry OccupancyGrid::GetUnit(cc::IVec2 tile) const
{
    CC_ASSERT(InBounds(tile));
    return units_[Index(tile)];
}

void OccupancyGrid::SetUnit(cc::IVec2 tile, Entity entity, std::uint32_t generation)
{
    CC_ASSERT(InBounds(tile));
    units_[Index(tile)] = { entity, generation };
}

Entity OccupancyGrid::GetBuilding(cc::IVec2 tile) const
{
    CC_ASSERT(InBounds(tile));
    return buildings_[Index(tile)];
}

void OccupancyGrid::SetBuilding(cc::IVec2 tile, Entity building)
{
    CC_ASSERT(InBounds(tile));
    buildings_[Index(tile)] = building;
}

void OccupancyGrid::ReserveFootprint(cc::IVec2 anchor, int footprintW, int footprintH,
                                     Entity entity, std::uint32_t generation)
{
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const cc::IVec2 tile{ anchor.x + dx, anchor.y + dy };
            if (InBounds(tile))
            {
                SetUnit(tile, entity, generation);
            }
        }
    }
}

void OccupancyGrid::ReleaseFootprint(cc::IVec2 anchor, int footprintW, int footprintH)
{
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const cc::IVec2 tile{ anchor.x + dx, anchor.y + dy };
            if (InBounds(tile))
            {
                SetUnit(tile, kOccEmpty, 0);
            }
        }
    }
}

void OccupancyGrid::ReleaseFootprintOwned(cc::IVec2 anchor, int footprintW, int footprintH,
                                          Entity entity, std::uint32_t generation)
{
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const cc::IVec2 tile{ anchor.x + dx, anchor.y + dy };
            if (InBounds(tile))
            {
                const OccEntry occ = GetUnit(tile);
                if (occ.entity == entity && occ.generation == generation)
                {
                    SetUnit(tile, kOccEmpty, 0);
                }
            }
        }
    }
}

int OccupancyGrid::ReserveFootprintOwned(cc::IVec2 anchor, int footprintW, int footprintH,
                                         Entity entity, std::uint32_t generation)
{
    int reserved = 0;
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const cc::IVec2 tile{ anchor.x + dx, anchor.y + dy };
            if (InBounds(tile))
            {
                const OccEntry occ = GetUnit(tile);
                if (occ.entity == kOccEmpty ||
                    (occ.entity == entity && occ.generation == generation))
                {
                    SetUnit(tile, entity, generation);
                    ++reserved;
                }
            }
        }
    }
    return reserved;
}

bool OccupancyGrid::CanEnter(const TileMap &map, cc::IVec2 anchor, int footprintW, int footprintH,
                              Entity self, std::uint32_t selfGen) const
{
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const cc::IVec2 tile{ anchor.x + dx, anchor.y + dy };
            if (!InBounds(tile) || map.IsBlocked(tile))
            {
                return false;
            }
            const OccEntry occ = GetUnit(tile);
            if (occ.entity != kOccEmpty && !(occ.entity == self && occ.generation == selfGen))
            {
                return false;
            }
        }
    }
    return true;
}

void OccupancyGrid::Clear()
{
    std::fill(units_.begin(), units_.end(), OccEntry{});
    std::fill(buildings_.begin(), buildings_.end(), kOccEmpty);
}

void OccupancyGrid::Resize(int widthTiles, int heightTiles)
{
    CC_ASSERT(widthTiles > 0 && heightTiles > 0);
    std::size_t count = 0;
    if (!GridDimsValid(widthTiles, heightTiles, count))
    {
        return; // keep existing contents rather than corrupt on bad input
    }
    width_ = widthTiles;
    height_ = heightTiles;
    units_.assign(count, OccEntry{});
    buildings_.assign(count, kOccEmpty);
}

int OccupancyGrid::Index(cc::IVec2 tile) const
{
    return tile.y * width_ + tile.x;
}
