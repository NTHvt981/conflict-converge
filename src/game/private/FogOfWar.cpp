#include "FogOfWar.h"

#include "CcAssert.h"

#include <cmath>

namespace
{

// Scout-role types reveal further without stat changes (M9 goal 5).
bool IsScout(UnitType type)
{
    return type == UnitType::Infantry || type == UnitType::IFV;
}

constexpr float kScoutMultiplier = 1.5f;

} // namespace

void FogOfWar::Resize(int width, int height)
{
    CC_ASSERT(width > 0 && height > 0);
    width_ = width;
    height_ = height;
    explored_.clear();
    visible_.clear();
}

int FogOfWar::Width() const
{
    return width_;
}

int FogOfWar::Height() const
{
    return height_;
}

bool FogOfWar::InBounds(cc::IVec2 tile) const
{
    return tile.x >= 0 && tile.y >= 0 && tile.x < width_ && tile.y < height_;
}

std::vector<std::uint8_t> &FogOfWar::ExploredRow(int teamID)
{
    auto it = explored_.find(teamID);
    if (it == explored_.end())
    {
        it = explored_.emplace(teamID, std::vector<std::uint8_t>(
                                          static_cast<std::size_t>(width_) *
                                          static_cast<std::size_t>(height_), 0))
                 .first;
    }
    return it->second;
}

const std::vector<std::uint8_t> *FogOfWar::FindVisible(int teamID) const
{
    const auto it = visible_.find(teamID);
    return it == visible_.end() ? nullptr : &it->second;
}

void FogOfWar::Recompute(const Registry &registry)
{
    visible_.clear();
    if (width_ <= 0 || height_ <= 0)
    {
        return;
    }
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.health <= 0.0f || unit.sightRange <= 0.0f)
        {
            return; // corpses and the blind reveal nothing
        }
        float range = unit.sightRange;
        if (IsScout(unit.type))
        {
            range *= kScoutMultiplier;
        }
        const int radius =
            static_cast<int>(std::ceil(range / cc::TILE_SIZE));
        const cc::IVec2 center = cc::WorldToTile(cc::ToGlm(unit.position));
        auto it = visible_.find(unit.teamID);
        if (it == visible_.end())
        {
            it = visible_.emplace(unit.teamID, std::vector<std::uint8_t>(
                                                     static_cast<std::size_t>(width_) *
                                                     static_cast<std::size_t>(height_), 0))
                     .first;
        }
        std::vector<std::uint8_t> &grid = it->second;
        std::vector<std::uint8_t> &seen = ExploredRow(unit.teamID);
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (dx * dx + dy * dy > radius * radius)
                {
                    continue;
                }
                const cc::IVec2 tile{ center.x + dx, center.y + dy };
                if (!InBounds(tile))
                {
                    continue;
                }
                const std::size_t i = static_cast<std::size_t>(tile.y) *
                                          static_cast<std::size_t>(width_) +
                                      static_cast<std::size_t>(tile.x);
                grid[i] = 1;
                seen[i] = 1;
            }
        }
    });
}

bool FogOfWar::IsVisible(int teamID, cc::IVec2 tile) const
{
    if (!InBounds(tile))
    {
        return false;
    }
    const std::vector<std::uint8_t> *grid = FindVisible(teamID);
    return grid != nullptr &&
           (*grid)[static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(width_) +
                   static_cast<std::size_t>(tile.x)] != 0;
}

bool FogOfWar::IsExplored(int teamID, cc::IVec2 tile) const
{
    if (!InBounds(tile))
    {
        return false;
    }
    const auto it = explored_.find(teamID);
    return it != explored_.end() &&
           it->second[static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(width_) +
                      static_cast<std::size_t>(tile.x)] != 0;
}

std::vector<std::uint8_t> FogOfWar::ExploredBytes(int teamID) const
{
    const auto it = explored_.find(teamID);
    if (it == explored_.end())
    {
        return std::vector<std::uint8_t>(static_cast<std::size_t>(width_) *
                                         static_cast<std::size_t>(height_), 0);
    }
    return it->second;
}

void FogOfWar::SetExplored(int teamID, const std::uint8_t *bytes, std::size_t size)
{
    std::vector<std::uint8_t> &row = ExploredRow(teamID);
    if (size != row.size() || bytes == nullptr)
    {
        return; // size mismatch: keep current memory, never trust the input
    }
    for (std::size_t i = 0; i < size; ++i)
    {
        row[i] = (bytes[i] != 0) ? 1 : 0;
    }
}

std::vector<int> FogOfWar::Teams() const
{
    std::vector<int> teams;
    teams.reserve(explored_.size());
    for (const auto &entry : explored_)
    {
        teams.push_back(entry.first);
    }
    return teams;
}
