#include "Formation.h"

#include "Pathfinder.h"
#include "TileMap.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace formation
{

namespace
{
struct TileHash
{
    std::size_t operator()(cc::IVec2 tile) const
    {
        return (static_cast<std::size_t>(static_cast<std::uint32_t>(tile.x)) * 73856093u) ^
               (static_cast<std::size_t>(static_cast<std::uint32_t>(tile.y)) * 19349663u);
    }
};

std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> GroupByAnchor(
    Registry &registry, const std::vector<Entity> &units)
{
    std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> groups;
    for (Entity id : units)
    {
        if (const Unit *unit = registry.Get<Unit>(id))
        {
            groups[cc::WorldToTile(cc::ToGlm(unit->position))].push_back(id);
        }
    }
    return groups;
}

bool StaggerIfCoLocated(
    Registry &registry, Unit &unit, Entity self, const TileMap &map, const OccupancyGrid &occ,
    const std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> &groups,
    std::vector<cc::IVec2> &claimedEscapeTiles, Vector2 realSlotWorld)
{
    const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
    const auto it = groups.find(anchor);
    if (it == groups.end() || it->second.size() < 2)
    {
        return false;
    }
    Entity keeper = it->second[0];
    for (Entity id : it->second)
    {
        if (id < keeper)
        {
            keeper = id;
        }
    }
    if (self == keeper)
    {
        return false;
    }
    const std::uint32_t selfGen = registry.Generation(self);
    cc::IVec2 escape = NearestEnterableTile(map, occ, anchor, unit.footprintWidth,
                                            unit.footprintHeight, self, selfGen);
    for (int guard = 0; guard < 8 && std::find(claimedEscapeTiles.begin(),
                                               claimedEscapeTiles.end(),
                                               escape) != claimedEscapeTiles.end();
         ++guard)
    {
        escape = NearestEnterableTile(map, occ, escape + cc::IVec2(1, 0), unit.footprintWidth,
                                      unit.footprintHeight, self, selfGen);
    }
    claimedEscapeTiles.push_back(escape);
    IssuePathOrderFootprint(unit, map, occ, cc::ToRaylib(cc::TileToWorld(escape.x, escape.y)),
                           self, selfGen);
    unit.orderQueue.push_back(QueuedOrder{ QueuedOrderKind::Move, realSlotWorld, {}, kInvalidEntity });
    return true;
}
}

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
            continue;
        }
        const cc::IVec2 slot = anchor + offsets[i];
        IssuePathOrder(*unit, map, cc::ToRaylib(cc::TileToWorld(slot.x, slot.y)));
    }
}

void IssueFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                          const TileMap &map, const OccupancyGrid &occ,
                          Vector2 worldTarget, bool slowestSpeed)
{
    int cellSize = 1;
    float minSpeed = 0.0f;
    bool firstSpeed = true;
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
        if (firstSpeed || unit->speed < minSpeed)
        {
            minSpeed = unit->speed;
            firstSpeed = false;
        }
    }
    const std::vector<cc::IVec2> offsets = FormationOffsetsFP(units.size(), cellSize);
    const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(worldTarget));
    const std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> groups =
        GroupByAnchor(registry, units);
    std::vector<cc::IVec2> claimedEscapeTiles;

	OccupancyGrid formationOcc = occ;

    for (std::size_t i = 0; i < units.size(); ++i)
    {
        Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue;
        }
        ClearOrders(*unit);
        unit->orderQueue.clear();
        unit->speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;
        const cc::IVec2 slot = NearestEnterableTile(
            map, formationOcc, anchor + offsets[i], unit->footprintWidth, unit->footprintHeight,
            units[i], registry.Generation(units[i]));
        const Vector2 slotWorld = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));

        if (StaggerIfCoLocated(registry, *unit, units[i], map, formationOcc, groups, claimedEscapeTiles,
                               slotWorld))
        {
            continue;
        }
        IssuePathOrderFootprint(*unit, map, formationOcc, slotWorld, units[i],
                               registry.Generation(units[i]));

		if (unit->hasPath)
		{
			formationOcc.ReserveFootprintOwned(slot, unit->footprintWidth, unit->footprintHeight, units[i], registry.Generation(units[i]));
		}
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

void IssueLineFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                              const TileMap &map, const OccupancyGrid &occ,
                              Vector2 lineStartWorld, Vector2 lineEndWorld,
                              bool slowestSpeed)
{
    const std::vector<cc::Vec2> positions = LineFormationPositions(
        units.size(), cc::ToGlm(lineStartWorld), cc::ToGlm(lineEndWorld));
    float minSpeed = 0.0f;
    bool firstSpeed = true;
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        const Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue;
        }
        if (firstSpeed || unit->speed < minSpeed)
        {
            minSpeed = unit->speed;
            firstSpeed = false;
        }
    }
    const std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> groups =
        GroupByAnchor(registry, units);
    std::vector<cc::IVec2> claimedEscapeTiles;
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue;
        }
        ClearOrders(*unit);
        unit->orderQueue.clear();
        unit->speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;
        const cc::IVec2 want = cc::WorldToTile(positions[i]);
        const cc::IVec2 slot = NearestEnterableTile(
            map, occ, want, unit->footprintWidth, unit->footprintHeight, units[i],
            registry.Generation(units[i]));
        const Vector2 slotWorld = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));
        if (StaggerIfCoLocated(registry, *unit, units[i], map, occ, groups, claimedEscapeTiles,
                               slotWorld))
        {
            continue;
        }
        IssuePathOrderFootprint(*unit, map, occ, slotWorld, units[i],
                               registry.Generation(units[i]));
    }
}

}
