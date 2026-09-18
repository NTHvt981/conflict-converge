#pragma once

#include <cstdint>
#include <vector>

#include "CcAssert.h"
#include "MathUtils.h" // cc::IVec2, cc::TILE_SIZE

// Tile-based movement system. TileMap owns the 64x64 grid:
// dimensions, per-tile terrain, and blocked queries for movement,
// pathfinding, and building placement. World/tile conversion
// lives in MathUtils.h; this class only stores and answers about tiles.

enum class TerrainType : std::uint8_t
{
    Grass,    // passable, default fill
    Water,    // blocked (impassable)
    Building, // blocked (occupied by a structure)
    // Appended AFTER Building so old saves (values 0-2) still decode.
    Forest, // passable, cost multiplier ready (uniform 1.0)
    Rock,    // blocked (impassable wall tile for choke points)
    Count // keep last: save decode validates < Count
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

    // Blocked = Water, Building, or Rock. Forest is passable. Out-of-bounds
    // counts as blocked so movement stays inside the map without extra
    // edge checks.
    bool IsBlocked(cc::IVec2 tile) const;

    void Clear(TerrainType fill = TerrainType::Grass);

    // Resize to new dimensions, clearing to Grass (save/load support).
    void Resize(int widthTiles, int heightTiles);

private:
    int Index(cc::IVec2 tile) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<TerrainType> tiles_;
};

// Grid occupancy for multi-tile units and buildings.
// Stores per-tile entity references (unit or building) with generation
// counters so stale IDs from recycled entities are detected. Buildings
// use a separate buildingId map since they persist across unit lifetimes.

using Entity = std::uint32_t;
inline constexpr Entity kOccEmpty = 0;

struct OccEntry
{
    Entity entity = kOccEmpty;
    std::uint32_t generation = 0;
};

class OccupancyGrid
{
public:
    OccupancyGrid(int widthTiles, int heightTiles);

    int Width() const;
    int Height() const;
    bool InBounds(cc::IVec2 tile) const;

    // Per-tile unit occupancy (entity + generation for stale detection).
    OccEntry GetUnit(cc::IVec2 tile) const;
    void SetUnit(cc::IVec2 tile, Entity entity, std::uint32_t generation);

    // Per-tile building ownership (separate from unit occupancy).
    Entity GetBuilding(cc::IVec2 tile) const;
    void SetBuilding(cc::IVec2 tile, Entity building);

    // Reserve all tiles in a rectangular footprint for a unit.
    // anchor is the top-left tile; extends toward +x/+y.
    void ReserveFootprint(cc::IVec2 anchor, int footprintW, int footprintH,
                          Entity entity, std::uint32_t generation);

    // Release all tiles in a rectangular footprint.
    void ReleaseFootprint(cc::IVec2 anchor, int footprintW, int footprintH);

    // Ownership-checked variants for the per-frame pre-pass and teardown:
    // release clears only cells holding (entity, generation), reserve stamps
    // only free-or-self cells and never clobbers another unit's anchor.
    // Returns the number of cells reserved (0 when fully overlapped).
    // Partial reservation is routine in crowds — the movement pre-pass
    // deliberately ignores the count (an overlapped unit simply goes
    // unreserved until separation pushes it clear); the count exists for
    // callers and tests that need all-or-nothing.
    void ReleaseFootprintOwned(cc::IVec2 anchor, int footprintW, int footprintH,
                               Entity entity, std::uint32_t generation);
    int ReserveFootprintOwned(cc::IVec2 anchor, int footprintW, int footprintH,
                              Entity entity, std::uint32_t generation);

    // Check whether a unit's entire footprint can enter at anchor.
    // Returns true only if every tile in the footprint is in-bounds,
    // not terrain-blocked, and not occupied by another entity.
    bool CanEnter(const TileMap &map, cc::IVec2 anchor, int footprintW, int footprintH,
                  Entity self, std::uint32_t selfGen) const;

    // Reset all occupancy (unit + building).
    void Clear();

    // Resize (clears occupancy).
    void Resize(int widthTiles, int heightTiles);

private:
    int Index(cc::IVec2 tile) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<OccEntry> units_;
    std::vector<Entity> buildings_;
};
