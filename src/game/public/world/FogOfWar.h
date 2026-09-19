#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "MathUtils.h"
#include "Registry.h"
#include "Unit.h"

// Fog of war. Per-team visibility: `visible` is recomputed from living units'
// sightRange circles every Recompute; `explored` latches forever.

class FogOfWar
{
public:
    // Size the grids; clears all vision memory.
    void Resize(int width, int height);
    int Width() const;
    int Height() const;

    // Rebuild every team's visible set from unit positions, then fold into explored.
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
