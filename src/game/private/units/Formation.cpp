#include "Formation.h"

#include "Pathfinder.h" // IssuePathOrder, IssuePathOrderFootprint
#include "TileMap.h"

#include <algorithm>
#include <cmath>
#include <numeric> // std::accumulate
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

// Deadlock fix (see plans/StackedOrderDeadlock_Plan.md): units that share
// their CURRENT anchor tile at order-issue time would otherwise all try to
// step off that tile at once, each blocking the others until the retry
// budget silently cancels the order. Group units by current anchor tile
// once, up front, so IssueFormationMoveFP/IssueLineFormationMoveFP can
// stagger every group member but one.
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

// If `unit` shares its start tile with another unit in this same order
// (per `groups`), every member but the lowest-Entity "keeper" gets an
// immediate escape hop to the nearest enterable tile away from the shared
// origin, with its real slot queued behind it (OnOrderFinished dispatches
// the queue entry automatically on arrival) -- so it starts moving this
// frame instead of contesting the shared tile with its stack-mate and
// risking a silent cancel a couple seconds later. Returns true if it
// staggered (caller must not also issue the real order for this unit).
bool StaggerIfCoLocated(
    Registry &registry, Unit &unit, Entity self, const TileMap &map, const OccupancyGrid &occ,
    const std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> &groups,
    std::vector<cc::IVec2> &claimedEscapeTiles, Vector2 realSlotWorld)
{
    const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
    const auto it = groups.find(anchor);
    if (it == groups.end() || it->second.size() < 2)
    {
        return false; // not sharing a tile with anyone else in this order
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
        return false; // the keeper gets its real order directly, unchanged
    }
    const std::uint32_t selfGen = registry.Generation(self);
    cc::IVec2 escape = NearestEnterableTile(map, occ, anchor, unit.footprintWidth,
                                            unit.footprintHeight, self, selfGen);
    // Same-frame guard: a 3rd+ member of this exact stack must not be handed
    // an escape tile another member already claimed a moment ago in this
    // same loop (their own occupancy reservations don't exist yet -- none
    // of them have moved).
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
} // namespace

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
                          Vector2 worldTarget, bool slowestSpeed)
{
    // Determine the maximum footprint dimension for cell sizing.
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
    for (std::size_t i = 0; i < units.size(); ++i)
    {
        Unit *unit = registry.Get<Unit>(units[i]);
        if (unit == nullptr)
        {
            continue;
        }
        // Fresh squad order: drop any stale order-type flag (repair/
        // attack-ground/patrol) so it can't swallow this move — see
        // ClearOrders in Unit.h. The cap line below then overwrites with
        // the real value; the queue clear below keeps this function
        // self-contained (same as IssueLineFormationMoveFP) so a future
        // caller can't reintroduce the gap.
        ClearOrders(*unit);
        unit->orderQueue.clear();
        // QoL slowest-speed: the whole squad marches at the minimum, set
        // before the path order goes out (cleared: every other path assigns
        // -1 explicitly so a stale cap never survives a fresh order).
        unit->speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;
        // Slots landing on units sanitize to the nearest enterable anchor
        // so squadmates don't all cancel against the same blocker.
        const cc::IVec2 slot = NearestEnterableTile(
            map, occ, anchor + offsets[i], unit->footprintWidth, unit->footprintHeight,
            units[i], registry.Generation(units[i]));
        const Vector2 slotWorld = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));
        // Deadlock fix: units still sharing a start tile get staggered
        // instead of all issued a real order that would contest it (see
        // StaggerIfCoLocated / plans/StackedOrderDeadlock_Plan.md).
        if (StaggerIfCoLocated(registry, *unit, units[i], map, occ, groups, claimedEscapeTiles,
                               slotWorld))
        {
            continue;
        }
        IssuePathOrderFootprint(*unit, map, occ, slotWorld, units[i],
                               registry.Generation(units[i]));
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
            continue; // destroyed/missing IDs don't shift the surviving slots
        }
        // Fresh line order: same flag + queue clear as above, but
        // self-contained — this function's callers don't clear the queue
        // (unlike the squad-formation call site), so it does its own
        // bookkeeping and the gap can't be reintroduced.
        ClearOrders(*unit);
        unit->orderQueue.clear();
        unit->speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;
        const cc::IVec2 want = cc::WorldToTile(positions[i]);
        const cc::IVec2 slot = NearestEnterableTile(
            map, occ, want, unit->footprintWidth, unit->footprintHeight, units[i],
            registry.Generation(units[i]));
        const Vector2 slotWorld = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));
        // Deadlock fix: see IssueFormationMoveFP above.
        if (StaggerIfCoLocated(registry, *unit, units[i], map, occ, groups, claimedEscapeTiles,
                               slotWorld))
        {
            continue;
        }
        IssuePathOrderFootprint(*unit, map, occ, slotWorld, units[i],
                               registry.Generation(units[i]));
    }
}

} // namespace formation
