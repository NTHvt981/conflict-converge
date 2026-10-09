#include "world/Pathfinder.h"

#include "units/Extensions.h"

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

const cc::IVec2 kDirs8[] = {
    { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 },
    { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
};

template <typename Enter>
TilePath FindPathCore(const TileMap &map, cc::IVec2 start, cc::IVec2 goal, Enter enter)
{
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
            float stepCost = 0.0f;
            if (!enter(current, dir, next, stepCost))
            {
                continue;
            }
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

float FootprintCost(const TileMap &map, cc::IVec2 anchor, int footprintW, int footprintH,
                    Locomotor locomotor)
{
    float cost = 0.0f;
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const float tile = TerrainCost(map.Get({ anchor.x + dx, anchor.y + dy }), locomotor);
            if (tile <= 0.0f)
            {
                return 0.0f;
            }
            if (tile > cost)
            {
                cost = tile;
            }
        }
    }
    return cost;
}

}

Locomotor LocomotorOf(UnitType type)
{
    switch (type)
    {
    case UnitType::IFV:
    case UnitType::Artillery:
        return Locomotor::Wheeled;
    case UnitType::LightTank:
    case UnitType::HeavyTank:
        return Locomotor::Tracked;
    default:
        return Locomotor::Foot;
    }
}

float TerrainCost(TerrainType terrain, Locomotor locomotor)
{
    switch (terrain)
    {
    case TerrainType::Grass:
        return 1.0f;
    case TerrainType::Forest:
        return locomotor == Locomotor::Foot ? 1.0f
             : locomotor == Locomotor::Tracked ? 1.8f
                                               : 1.6f;
    default:
        return 0.0f;
    }
}

float TerrainCost(TerrainType terrain)
{
    return TerrainCost(terrain, Locomotor::Tracked);
}

TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal,
                  Locomotor locomotor)
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

    auto enter = [&](cc::IVec2 current, cc::IVec2 dir, cc::IVec2 next, float &cost) {
        if (!map.InBounds(next) || map.IsBlocked(next))
        {
            return false;
        }
        if (dir.x != 0 && dir.y != 0)
        {
            const cc::IVec2 cardA{ current.x + dir.x, current.y };
            const cc::IVec2 cardB{ current.x, current.y + dir.y };
            if (!map.InBounds(cardA) || map.IsBlocked(cardA) || !map.InBounds(cardB) ||
                map.IsBlocked(cardB))
            {
                return false;
            }
        }
        const float terrain = TerrainCost(map.Get(next), locomotor);
        if (terrain <= 0.0f)
        {
            return false;
        }
        cost = ((dir.x != 0 && dir.y != 0) ? 1.41421356f : 1.0f) * terrain;
        return true;
    };
    return FindPathCore(map, start, goal, enter);
}

TilePath FindPathFootprint(const TileMap &map, const OccupancyGrid &occ,
                           cc::IVec2 start, cc::IVec2 goal,
                           int footprintW, int footprintH,
                           Entity self, std::uint32_t selfGen,
                           Locomotor locomotor)
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

    auto enter = [&](cc::IVec2 current, cc::IVec2 dir, cc::IVec2 next, float &cost) {
        if (dir.x != 0 && dir.y != 0)
        {
            const cc::IVec2 cardA{ current.x + dir.x, current.y };
            const cc::IVec2 cardB{ current.x, current.y + dir.y };
            if (!occ.CanEnter(map, cardA, footprintW, footprintH, self, selfGen) ||
                !occ.CanEnter(map, cardB, footprintW, footprintH, self, selfGen))
            {
                return false;
            }
        }
        if (!occ.CanEnter(map, next, footprintW, footprintH, self, selfGen))
        {
            return false;
        }
        const float terrain = FootprintCost(map, next, footprintW, footprintH, locomotor);
        if (terrain <= 0.0f)
        {
            return false;
        }
        cost = ((dir.x != 0 && dir.y != 0) ? 1.41421356f : 1.0f) * terrain;
        return true;
    };
    return FindPathCore(map, start, goal, enter);
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

namespace
{

bool FootprintFree(const TileMap &map, const OccupancyGrid *occ, cc::IVec2 anchor,
                   int footprintW, int footprintH)
{
    for (int dy = 0; dy < footprintH; ++dy)
    {
        for (int dx = 0; dx < footprintW; ++dx)
        {
            const cc::IVec2 tile{ anchor.x + dx, anchor.y + dy };
            if (!map.InBounds(tile) || map.IsBlocked(tile))
            {
                return false;
            }
        }
    }
    if (occ != nullptr && !occ->CanEnter(map, anchor, footprintW, footprintH, kInvalidEntity, 0))
    {
        return false;
    }
    return true;
}

}

cc::IVec2 NearestFreeFootprintTile(const TileMap &map, const OccupancyGrid *occ,
                                   cc::IVec2 want, int footprintW, int footprintH)
{
    if (FootprintFree(map, occ, want, footprintW, footprintH))
    {
        return want;
    }
    for (int ring = 1; ring <= 8; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (dx > -ring && dx < ring && dy > -ring && dy < ring)
                {
                    continue;
                }
                const cc::IVec2 anchor{ want.x + dx, want.y + dy };
                if (FootprintFree(map, occ, anchor, footprintW, footprintH))
                {
                    return anchor;
                }
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
    TilePath path = FindPath(map, start, goal, LocomotorOf(unit.type));
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
                                      self, selfGen, LocomotorOf(unit.type));
    if (path.empty())
    {
        IssueMoveOrder(unit, orders, mover, cc::ToRaylib(cc::TileToWorld(goal.x, goal.y)));
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
