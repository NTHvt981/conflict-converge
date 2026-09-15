#include "Formation.h"

#include "Pathfinder.h" // IssuePathOrder, IssuePathOrderFootprint
#include "TileMap.h"

#include <cmath>
#include <numeric> // std::accumulate

namespace formation
{

std::vector<cc::IVec2> FormationOffsets(std::size_t count)
{
    std::vector<cc::IVec2> offsets;
    if (count == 0)
    {
        return offsets;
    }
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    offsets.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        offsets.push_back({ static_cast<int>(i % static_cast<std::size_t>(cols)),
                            static_cast<int>(i / static_cast<std::size_t>(cols)) });
    }
    return offsets;
}

std::vector<cc::IVec2> FormationOffsetsFP(std::size_t count, int cellSize)
{
    std::vector<cc::IVec2> offsets;
    if (count == 0 || cellSize <= 0)
    {
        return offsets;
    }
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    offsets.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        offsets.push_back({ static_cast<int>(i % static_cast<std::size_t>(cols)) * cellSize,
                            static_cast<int>(i / static_cast<std::size_t>(cols)) * cellSize });
    }
    return offsets;
}

void IssueFormationMove(Registry &registry, const std::vector<Entity> &units, const TileMap &map,
                        Vector2 worldTarget)
{
    const std::vector<cc::IVec2> offsets = FormationOffsets(units.size());
    const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(worldTarget));
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue; // destroyed/missing IDs don't shift the surviving slots
        }
        const cc::IVec2 slot = anchor + offsets[i];
        IssuePathOrder(*unit, map, cc::ToRaylib(cc::TileToWorld(slot.x, slot.y)));
    }
}

void IssueFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                          const TileMap &map, const OccupancyGrid &occ,
                          Vector2 worldTarget)
{
    // Determine the maximum footprint dimension for cell sizing.
    int cellSize = 1;
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        const Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue;
        }
        const int dim = (unit->footprintWidth > unit->footprintHeight) ? unit->footprintWidth
                                                                       : unit->footprintHeight;
        if (dim > cellSize)
        {
            cellSize = dim;
        }
    }
    const std::vector<cc::IVec2> offsets = FormationOffsetsFP(units.size(), cellSize);
    const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(worldTarget));
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue;
        }
        // Slots landing on units sanitize to the nearest enterable anchor
        // so squadmates don't all cancel against the same blocker.
        const cc::IVec2 slot = NearestEnterableTile(
            map, occ, anchor + offsets[i], unit->footprintWidth, unit->footprintHeight,
            units[i], registry.Generation(units[i]));
        IssuePathOrderFootprint(*unit, map, occ,
                                cc::ToRaylib(cc::TileToWorld(slot.x, slot.y)),
                                units[i], registry.Generation(units[i]));
    }
}

} // namespace formation
