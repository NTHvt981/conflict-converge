#include "Formation.h"

#include "Extensions.h"
#include "Pathfinder.h"
#include "TileMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

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
    IssuePathOrderFootprint(unit, GetOrders(registry, self), GetMover(registry, self), map, occ,
                            cc::ToRaylib(cc::TileToWorld(escape.x, escape.y)), self, selfGen);
    GetOrders(registry, self).orderQueue.push_back(
        QueuedOrder{ QueuedOrderKind::Move, realSlotWorld, {}, kInvalidEntity });
    return true;
}

// Octile tile distance scaled by 10 (orthogonal 10, diagonal 14), matching
// the 8-directional footprint pathfinder's step costs. Cheap heuristic used
// for slot assignment; the issued orders still run full A*.
int OctileCost(cc::IVec2 a, cc::IVec2 b)
{
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    const int mn = (dx < dy) ? dx : dy;
    const int mx = (dx < dy) ? dy : dx;
    return 10 * mx + 4 * mn;
}

bool KuhnDfs(int u, const std::vector<std::vector<int>> &allowed, std::vector<int> &matchSlot,
             std::vector<char> &seen)
{
    for (int v : allowed[u])
    {
        if (seen[static_cast<std::size_t>(v)])
        {
            continue;
        }
        seen[static_cast<std::size_t>(v)] = 1;
        if (matchSlot[static_cast<std::size_t>(v)] < 0 ||
            KuhnDfs(matchSlot[static_cast<std::size_t>(v)], allowed, matchSlot, seen))
        {
            matchSlot[static_cast<std::size_t>(v)] = u;
            return true;
        }
    }
    return false;
}

// True when every unit can take a distinct slot with cost <= threshold.
// Iterates units/slots in index order so the result is deterministic.
bool CanMatchThreshold(const std::vector<std::vector<int>> &cost, int threshold)
{
    const int n = static_cast<int>(cost.size());
    std::vector<std::vector<int>> allowed(static_cast<std::size_t>(n));
    for (int u = 0; u < n; ++u)
    {
        for (int v = 0; v < n; ++v)
        {
            if (cost[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] <= threshold)
            {
                allowed[static_cast<std::size_t>(u)].push_back(v);
            }
        }
    }
    std::vector<int> matchSlot(static_cast<std::size_t>(n), -1);
    int matched = 0;
    for (int u = 0; u < n; ++u)
    {
        std::vector<char> seen(static_cast<std::size_t>(n), 0);
        if (KuhnDfs(u, allowed, matchSlot, seen))
        {
            ++matched;
        }
    }
    return matched == n;
}

// Hungarian min-sum assignment restricted to edges with cost <= threshold.
// `cost` must admit a perfect matching under the threshold (guaranteed by
// the bottleneck search below). Iteration order is index order, so ties
// break deterministically: lower units prefer lower slots.
std::vector<int> HungarianRestricted(const std::vector<std::vector<int>> &cost, int threshold)
{
    const int n = static_cast<int>(cost.size());
    constexpr int kInf = 1000000000;
    // 1-indexed cost matrix; forbidden edges get INF.
    std::vector<std::vector<int>> a(static_cast<std::size_t>(n + 1),
                                    std::vector<int>(static_cast<std::size_t>(n + 1), 0));
    for (int i = 1; i <= n; ++i)
    {
        for (int j = 1; j <= n; ++j)
        {
            const int c = cost[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(j - 1)];
            a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
                (c <= threshold) ? c : kInf;
        }
    }
    std::vector<int> u(static_cast<std::size_t>(n + 1), 0), v(static_cast<std::size_t>(n + 1), 0),
        p(static_cast<std::size_t>(n + 1), 0), way(static_cast<std::size_t>(n + 1), 0);
    for (int i = 1; i <= n; ++i)
    {
        p[0] = i;
        int j0 = 0;
        std::vector<int> minv(static_cast<std::size_t>(n + 1), kInf);
        std::vector<char> used(static_cast<std::size_t>(n + 1), 0);
        do
        {
            used[static_cast<std::size_t>(j0)] = 1;
            const int i0 = p[static_cast<std::size_t>(j0)];
            int delta = kInf;
            int j1 = 0;
            for (int j = 1; j <= n; ++j)
            {
                if (used[static_cast<std::size_t>(j)])
                {
                    continue;
                }
                const int cur =
                    a[static_cast<std::size_t>(i0)][static_cast<std::size_t>(j)] -
                    u[static_cast<std::size_t>(i0)] - v[static_cast<std::size_t>(j)];
                if (cur < minv[static_cast<std::size_t>(j)])
                {
                    minv[static_cast<std::size_t>(j)] = cur;
                    way[static_cast<std::size_t>(j)] = j0;
                }
                if (minv[static_cast<std::size_t>(j)] < delta)
                {
                    delta = minv[static_cast<std::size_t>(j)];
                    j1 = j;
                }
            }
            for (int j = 0; j <= n; ++j)
            {
                if (used[static_cast<std::size_t>(j)])
                {
                    u[static_cast<std::size_t>(p[static_cast<std::size_t>(j)])] += delta;
                    v[static_cast<std::size_t>(j)] -= delta;
                }
                else
                {
                    minv[static_cast<std::size_t>(j)] -= delta;
                }
            }
            j0 = j1;
        } while (p[static_cast<std::size_t>(j0)] != 0);
        do
        {
            const int j1 = way[static_cast<std::size_t>(j0)];
            p[static_cast<std::size_t>(j0)] = p[static_cast<std::size_t>(j1)];
            j0 = j1;
        } while (j0 != 0);
    }
    // p[j] = assigned row for column j; invert to row -> column.
    std::vector<int> assignment(static_cast<std::size_t>(n), -1);
    for (int j = 1; j <= n; ++j)
    {
        if (p[static_cast<std::size_t>(j)] > 0)
        {
            assignment[static_cast<std::size_t>(p[static_cast<std::size_t>(j)] - 1)] = j - 1;
        }
    }
    return assignment;
}

// Bottleneck (minimize the longest unit->slot walk) with min-sum tie-break.
// Returns slot index per unit index. Empty for n == 0.
std::vector<int> BottleneckAssignment(const std::vector<std::vector<int>> &cost)
{
    const int n = static_cast<int>(cost.size());
    if (n == 0)
    {
        return {};
    }
    if (n == 1)
    {
        return { 0 };
    }
    std::vector<int> values;
    values.reserve(static_cast<std::size_t>(n * n));
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            values.push_back(cost[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
        }
    }
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    int lo = 0;
    int hi = static_cast<int>(values.size()) - 1;
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (CanMatchThreshold(cost, values[static_cast<std::size_t>(mid)]))
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return HungarianRestricted(cost, values[static_cast<std::size_t>(lo)]);
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
        IssuePathOrder(*unit, GetOrders(registry, units[i]), GetMover(registry, units[i]), map,
                       cc::ToRaylib(cc::TileToWorld(slot.x, slot.y)));
    }
}

void IssueFormationMoveFP(Registry &registry, const std::vector<Entity> &units,
                          const TileMap &map, const OccupancyGrid &occ,
                          Vector2 worldTarget, bool slowestSpeed)
{
    struct ValidUnit
    {
        Entity id = kInvalidEntity;
        Unit *unit = nullptr;
        cc::IVec2 anchorTile{};
    };
    std::vector<ValidUnit> valid;
    valid.reserve(units.size());
    for (Entity id : units)
    {
        Unit *unit = registry.Get<Unit>(id);
        if (unit == nullptr)
        {
            continue;
        }
        valid.push_back({ id, unit, cc::WorldToTile(cc::ToGlm(unit->position)) });
    }
    if (valid.empty())
    {
        return;
    }
    // Selection order is ignored: sort by entity so the assignment is a pure
    // function of unit positions, independent of pick order.
    std::sort(valid.begin(), valid.end(),
              [](const ValidUnit &a, const ValidUnit &b) { return a.id < b.id; });
    const int n = static_cast<int>(valid.size());

    int cellSize = 1;
    float minSpeed = 0.0f;
    bool firstSpeed = true;
    for (const ValidUnit &entry : valid)
    {
        const Unit *unit = entry.unit;
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
    const std::vector<cc::IVec2> offsets =
        FormationOffsetsFP(static_cast<std::size_t>(n), cellSize);
    const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(worldTarget));
    std::vector<cc::IVec2> baseSlots(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        baseSlots[static_cast<std::size_t>(i)] = anchor + offsets[static_cast<std::size_t>(i)];
    }
    // Bottleneck assignment (minimize the longest walk, min-sum tie-break)
    // over octile start->base-slot costs.
    std::vector<std::vector<int>> cost(static_cast<std::size_t>(n),
                                       std::vector<int>(static_cast<std::size_t>(n), 0));
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            cost[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
                OctileCost(valid[static_cast<std::size_t>(i)].anchorTile,
                           baseSlots[static_cast<std::size_t>(j)]);
        }
    }
    const std::vector<int> assignment = BottleneckAssignment(cost);

    const std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> groups =
        GroupByAnchor(registry, units);
    std::vector<cc::IVec2> claimedEscapeTiles;

	OccupancyGrid formationOcc = occ;

    for (int i = 0; i < n; ++i)
    {
        Unit *unit = valid[static_cast<std::size_t>(i)].unit;
        const Entity self = valid[static_cast<std::size_t>(i)].id;
        Orders &orders = GetOrders(registry, self);
        Mover &mover = GetMover(registry, self);
        ClearOrders(*unit, orders, mover);
        orders.orderQueue.clear();
        mover.speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;
        const cc::IVec2 want = baseSlots[static_cast<std::size_t>(
            assignment[static_cast<std::size_t>(i)])];
        const cc::IVec2 slot = NearestEnterableTile(
            map, formationOcc, want, unit->footprintWidth, unit->footprintHeight,
            self, registry.Generation(self));
        const Vector2 slotWorld = cc::ToRaylib(cc::TileToWorld(slot.x, slot.y));

        if (StaggerIfCoLocated(registry, *unit, self, map, formationOcc, groups, claimedEscapeTiles,
                               slotWorld))
        {
            continue;
        }
        IssuePathOrderFootprint(*unit, orders, mover, map, formationOcc, slotWorld, self,
                               registry.Generation(self));

		if (mover.hasPath)
		{
			formationOcc.ReserveFootprintOwned(slot, unit->footprintWidth, unit->footprintHeight, self, registry.Generation(self));
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
        Orders &orders = GetOrders(registry, units[i]);
        Mover &mover = GetMover(registry, units[i]);
        ClearOrders(*unit, orders, mover);
        orders.orderQueue.clear();
        mover.speedCapPixelsPerSec = slowestSpeed ? minSpeed : -1.0f;
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
        IssuePathOrderFootprint(*unit, orders, mover, map, occ, slotWorld, units[i],
                               registry.Generation(units[i]));
    }
}

}
