#include "units/Formation.h"

#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"

#include <vector>

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
            continue;
        }
        const cc::IVec2 slot = anchor + offsets[i];
        IssuePathOrder(*unit, GetOrders(registry, units[i]), GetMover(registry, units[i]), map,
                       cc::ToRaylib(cc::TileToWorld(slot.x, slot.y)));
    }
}


std::vector<cc::Vec2> LineFormationPositions(std::size_t count, cc::Vec2 lineStart,
                                             cc::Vec2 lineEnd)
{
    std::vector<cc::Vec2> positions;
    if (count == 0)
    {
        return positions;
    }
    positions.reserve(count);
    if (count == 1)
    {
        positions.push_back({ (lineStart.x + lineEnd.x) / 2.0f,
                              (lineStart.y + lineEnd.y) / 2.0f });
        return positions;
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        positions.push_back({ lineStart.x + (lineEnd.x - lineStart.x) * t,
                              lineStart.y + (lineEnd.y - lineStart.y) * t });
    }
    return positions;
}


}
