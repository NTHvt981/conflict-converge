#include "Formation.h"

#include "Pathfinder.h" // IssuePathOrder (A* with straight fallback)
#include "TileMap.h"

#include <cmath>

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

} // namespace formation
