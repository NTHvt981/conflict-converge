#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Registry.h"  // Entity iteration for vision sources
#include "Unit.h"      // sightRange, scout-role types

// Fog of war. Per-team visibility over the TileMap: `visible` is
// recomputed from living units' sightRange circles every Recompute call,
// `explored` latches forever (frozen StarCraft-style snapshot).
// Sight is pure radius — terrain never blocks. Infantry and IFV are
// scout-role and reveal at 1.5x range without touching BaseStats.

class FogOfWar
{
public:
    // Size the grids; clears all vision memory. Width/height must be positive.
    void Resize(int width, int height);
    int Width() const;
    int Height() const;

    // Rebuild every team's visible set from current unit positions, then
    // fold into explored. Cheap at this scale; stagger teams across frames
    // if unit counts ever make it show up in profiles.
    void Recompute(const Registry &registry);

    bool IsVisible(int teamID, cc::IVec2 tile) const;
    bool IsExplored(int teamID, cc::IVec2 tile) const;

    // Save/load support (team_fog): row-major explored bytes, w*h long.
    std::vector<std::uint8_t> ExploredBytes(int teamID) const;
    void SetExplored(int teamID, const std::uint8_t *bytes, std::size_t size);
    // Teams currently tracked (have had at least one living unit).
    std::vector<int> Teams() const;

private:
    bool InBounds(cc::IVec2 tile) const;
    std::vector<std::uint8_t> &ExploredRow(int teamID);
    const std::vector<std::uint8_t> *FindVisible(int teamID) const;

    int width_ = 0;
    int height_ = 0;
    std::unordered_map<int, std::vector<std::uint8_t>> explored_;
    std::unordered_map<int, std::vector<std::uint8_t>> visible_;
};
