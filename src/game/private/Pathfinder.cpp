#include "Pathfinder.h"

#include <queue>
#include <unordered_map>
#include <utility>

namespace
{

int TileKey(cc::IVec2 tile, int width)
{
    return tile.y * width + tile.x;
}

int Manhattan(cc::IVec2 a, cc::IVec2 b)
{
    const int dx = a.x >= b.x ? a.x - b.x : b.x - a.x;
    const int dy = a.y >= b.y ? a.y - b.y : b.y - a.y;
    return dx + dy;
}

struct OpenNode
{
    int priority = 0;
    cc::IVec2 tile{ 0, 0 };
};

// std::priority_queue is a max-heap: invert the comparison to pop lowest cost.
bool operator<(const OpenNode &a, const OpenNode &b)
{
    return a.priority > b.priority;
}

// Fixed neighbor order keeps routes deterministic for tests.
const cc::IVec2 kDirs[] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

} // namespace

TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal)
{
    if (map.Width() <= 0 || map.Height() <= 0)
    {
        return {};
    }
    if (!map.InBounds(start) || !map.InBounds(goal))
    {
        return {};
    }
    if (map.IsBlocked(start) || map.IsBlocked(goal))
    {
        return {};
    }
    if (start == goal)
    {
        return { start };
    }

    const int width = map.Width();
    std::priority_queue<OpenNode> open;
    open.push({ Manhattan(start, goal), start });

    std::unordered_map<int, int> costSoFar; // tile key -> g cost
    std::unordered_map<int, cc::IVec2> cameFrom; // tile key -> previous tile
    costSoFar[TileKey(start, width)] = 0;

    while (!open.empty())
    {
        const cc::IVec2 current = open.top().tile;
        open.pop();
        if (current == goal)
        {
            break;
        }

        for (cc::IVec2 dir : kDirs)
        {
            const cc::IVec2 next{ current.x + dir.x, current.y + dir.y };
            if (!map.InBounds(next) || map.IsBlocked(next))
            {
                continue;
            }
            const int nextKey = TileKey(next, width);
            const int newCost = costSoFar[TileKey(current, width)] + 1;
            const auto existing = costSoFar.find(nextKey);
            if (existing == costSoFar.end() || newCost < existing->second)
            {
                costSoFar[nextKey] = newCost;
                cameFrom[nextKey] = current;
                open.push({ newCost + Manhattan(next, goal), next });
            }
        }
    }

    if (cameFrom.find(TileKey(goal, width)) == cameFrom.end())
    {
        return {}; // goal never reached
    }

    TilePath reversed;
    cc::IVec2 cursor = goal;
    while (!(cursor == start))
    {
        reversed.push_back(cursor);
        cursor = cameFrom[TileKey(cursor, width)];
    }
    reversed.push_back(start);
    return TilePath(reversed.rbegin(), reversed.rend());
}

void IssuePathOrder(Unit &unit, const TileMap &map, Vector2 worldTarget)
{
    const cc::IVec2 start = cc::WorldToTile(cc::ToGlm(unit.position));
    const cc::IVec2 goal = cc::WorldToTile(cc::SnapToTile(cc::ToGlm(worldTarget)));
    TilePath path = FindPath(map, start, goal);
    if (path.empty())
    {
        IssueMoveOrder(unit, worldTarget); // unreachable: M2 straight attempt
        unit.hasPath = false;
        unit.path.clear();
        unit.pathNext = 0;
        return;
    }

    unit.moveTarget = cc::ToRaylib(cc::TileToWorld(goal.x, goal.y));
    unit.hasMoveOrder = true;
    unit.path = std::move(path);
    // The first node is the unit's own tile: start walking at the next one.
    unit.pathNext = 1;
    unit.hasPath = true;
}
