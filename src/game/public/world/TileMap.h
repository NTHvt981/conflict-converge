#pragma once

#include <cstdint>
#include <vector>

#include "CcAssert.h"
#include "MathUtils.h"

// Tile-based movement system. TileMap owns the 64x64 grid: dimensions,
// per-tile terrain, and blocked queries for movement, pathfinding, and
// building placement.

enum class TerrainType : std::uint8_t
{
    Grass,    // passable, default fill
    Water,    // blocked (impassable)
    Building, // blocked (occupied by a structure)
    Forest,   // passable, cost multiplier ready (uniform 1.0)
    Rock,     // blocked (impassable wall tile for choke points)
    Count
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

    // Blocked = Water, Building, or Rock; out-of-bounds counts as blocked.
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

// Grid occupancy for multi-tile units and buildings, with generation
// counters so stale IDs from recycled entities are detected.

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

    // Reserve/release a rectangular footprint (anchor = top-left tile).
    void ReserveFootprint(cc::IVec2 anchor, int footprintW, int footprintH,
                          Entity entity, std::uint32_t generation);
    void ReleaseFootprint(cc::IVec2 anchor, int footprintW, int footprintH);

    // Ownership-checked variants: release clears only cells holding
    // (entity, generation), reserve stamps only free-or-self cells.
    void ReleaseFootprintOwned(cc::IVec2 anchor, int footprintW, int footprintH,
                               Entity entity, std::uint32_t generation);
    int ReserveFootprintOwned(cc::IVec2 anchor, int footprintW, int footprintH,
                              Entity entity, std::uint32_t generation);

	void ReleaseAllUnitFootprints();

    // True only if every footprint tile is in-bounds, unblocked, and unoccupied.
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
