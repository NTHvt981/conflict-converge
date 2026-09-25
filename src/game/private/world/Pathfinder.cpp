#include "Pathfinder.h"

#include "Extensions.h"

#include <cmath>
#include <queue>
#include <unordered_map>
#include <utility>
#include <set>

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

float OctileDist(cc::IVec2 a, cc::IVec2 b)
{
    const float dx = static_cast<float>(a.x >= b.x ? a.x - b.x : b.x - a.x);
    const float dy = static_cast<float>(a.y >= b.y ? a.y - b.y : b.y - a.y);
    constexpr float kSqrt2 = 1.41421356f;
    return dx + dy + (kSqrt2 - 1.0f) * (dx < dy ? dx : dy);
}

struct OpenNode
{
    float priority = 0.0f;
    cc::IVec2 tile{ 0, 0 };
};

bool operator<(const OpenNode &a, const OpenNode &b)
{
    return a.priority > b.priority;
}

const cc::IVec2 kDirs[] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

const cc::IVec2 kDirs8[] = {
    { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 },
    { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
};

}

float TerrainCost(TerrainType terrain)
{
    (void)terrain;
    return 1.0f;
}

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
    open.push({ static_cast<float>(Manhattan(start, goal)), start });

    std::unordered_map<int, float> costSoFar;
    std::unordered_map<int, cc::IVec2> cameFrom;
    costSoFar[TileKey(start, width)] = 0.0f;

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
            const float newCost =
                costSoFar[TileKey(current, width)] + TerrainCost(map.Get(next));
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
        return {};
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

TilePath FindPathFootprint(const TileMap &map, const OccupancyGrid &occ,
                           cc::IVec2 start, cc::IVec2 goal,
                           int footprintW, int footprintH,
                           Entity self, std::uint32_t selfGen)
{
    if (map.Width() <= 0 || map.Height() <= 0)
    {
        return {};
    }
    if (!map.InBounds(start) || !map.InBounds(goal))
    {
        return {};
    }
    if (start == goal)
    {
        return { start };
    }

    const int width = map.Width();
    std::priority_queue<OpenNode> open;
    open.push({ OctileDist(start, goal), start });

    std::unordered_map<int, float> costSoFar;
    std::unordered_map<int, cc::IVec2> cameFrom;
    costSoFar[TileKey(start, width)] = 0.0f;

    while (!open.empty())
    {
        const cc::IVec2 current = open.top().tile;
        open.pop();
        if (current == goal)
        {
            break;
        }

        for (cc::IVec2 dir : kDirs8)
        {
            const cc::IVec2 next{ current.x + dir.x, current.y + dir.y };

            if (dir.x != 0 && dir.y != 0)
            {
                const cc::IVec2 cardA{ current.x + dir.x, current.y };
                const cc::IVec2 cardB{ current.x, current.y + dir.y };
                if (!occ.CanEnter(map, cardA, footprintW, footprintH, self, selfGen) ||
                    !occ.CanEnter(map, cardB, footprintW, footprintH, self, selfGen))
                {
                    continue;
                }
            }

            if (!occ.CanEnter(map, next, footprintW, footprintH, self, selfGen))
            {
                continue;
            }

            const float stepCost = (dir.x != 0 && dir.y != 0) ? 1.41421356f : 1.0f;
            const int nextKey = TileKey(next, width);
            const float newCost = costSoFar[TileKey(current, width)] + stepCost;
            const auto existing = costSoFar.find(nextKey);
            if (existing == costSoFar.end() || newCost < existing->second)
            {
                costSoFar[nextKey] = newCost;
                cameFrom[nextKey] = current;
                open.push({ newCost + OctileDist(next, goal), next });
            }
        }
    }

    if (cameFrom.find(TileKey(goal, width)) == cameFrom.end())
    {
        return {};
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

cc::IVec2 NearestEnterableTile(const TileMap &map, const OccupancyGrid &occ,
                               cc::IVec2 want, int footprintW, int footprintH,
                               Entity self, std::uint32_t selfGen)
{
    if (occ.CanEnter(map, want, footprintW, footprintH, self, selfGen))
    {
        return want;
    }

	for (int ring = 1; ring <= 5; ring++)
	{
		std::set<std::vector<int>> adjacentTiles;
		for (int i = 0; i <= ring; i++)
		{
			int reverse_i = ring - i;
			adjacentTiles.insert({ want.x + i, want.y + reverse_i });
			adjacentTiles.insert({ want.x - i, want.y - reverse_i });
			adjacentTiles.insert({ want.x - i, want.y + reverse_i });
			adjacentTiles.insert({ want.x + i, want.y - reverse_i });
		}

		for (const std::vector<int>& tile : adjacentTiles)
		{
			if (occ.CanEnter(map, { tile[0], tile[1] }, footprintW, footprintH, self, selfGen))
			{
				return { tile[0], tile[1] };
			}
		}
	}

    return want;
}

void IssuePathOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                    Vector2 worldTarget)
{
    const cc::IVec2 start = cc::WorldToTile(cc::ToGlm(unit.position));
    const cc::IVec2 goal = cc::WorldToTile(cc::SnapToTile(cc::ToGlm(worldTarget)));
    TilePath path = FindPath(map, start, goal);
    if (path.empty())
    {
        IssueMoveOrder(unit, orders, mover, worldTarget);
        mover.hasPath = false;
        mover.path.clear();
        mover.pathNext = 0;
        return;
    }

    mover.moveTarget = cc::ToRaylib(cc::TileToWorld(goal.x, goal.y));
    mover.hasMoveOrder = true;
    mover.path = std::move(path);
    mover.pathNext = 1;
    mover.hasPath = true;
    mover.blockedTime = 0.0f;
    mover.blockedRepaths = 0;
}

void IssuePathOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                             const OccupancyGrid &occ, Vector2 worldTarget, Entity self,
                             std::uint32_t selfGen)
{
    const cc::IVec2 start = cc::WorldToTile(cc::ToGlm(unit.position));
    cc::IVec2 goal = cc::WorldToTile(cc::SnapToTile(cc::ToGlm(worldTarget)));
    goal = NearestEnterableTile(map, occ, goal, unit.footprintWidth, unit.footprintHeight,
                                self, selfGen);
    TilePath path = FindPathFootprint(map, occ, start, goal,
                                      unit.footprintWidth, unit.footprintHeight,
                                      self, selfGen);
    if (path.empty())
    {
        mover.hasPath = false;
        mover.path.clear();
        mover.pathNext = 0;
        return;
    }

    mover.moveTarget = cc::ToRaylib(cc::TileToWorld(goal.x, goal.y));
    mover.hasMoveOrder = true;
    mover.path = std::move(path);
    mover.pathNext = 1;
    mover.hasPath = true;
    mover.blockedTime = 0.0f;
    mover.blockedRepaths = 0;
}
