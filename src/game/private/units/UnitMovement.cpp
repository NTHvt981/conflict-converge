
#include "units/Unit.h"

#include "units/Extensions.h"
#include "core/MathUtils.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/UnitInternal.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{

enum class StepResult
{
    Arrived,
    Blocked,
    BlockedUnit,
    Stepped,
};

StepResult StepToward(cc::Vec2 pos, cc::Vec2 target, float step, const TileMap &map, cc::Vec2 &outNext,
                      OccupancyGrid *occ = nullptr, Entity self = 0, std::uint32_t selfGen = 0,
                      int footprintW = 1, int footprintH = 1)
{
    if (step <= 0.0f)
    {
        return StepResult::BlockedUnit;
    }
    const cc::Vec2 diff = target - pos;
    const float dist = glm::length(diff);
    if (dist <= step)
    {
        // Treat off-map as blocked
        const cc::IVec2 fromTile = cc::WorldToTile(pos);
        const cc::IVec2 targetTile = cc::WorldToTile(target);
        if (targetTile != fromTile && map.IsBlocked(targetTile))
        {
            return StepResult::Blocked;
        }
        outNext = target;
        return StepResult::Arrived;
    }

    const cc::Vec2 next = pos + diff / dist * step;
    const cc::IVec2 from = cc::WorldToTile(pos);
    const cc::IVec2 to = cc::WorldToTile(next);
    if (map.IsBlocked(to) && to != from)
    {
        return StepResult::Blocked;
    }
    if (occ != nullptr && to != from)
    {
        if (!occ->CanEnter(map, to, footprintW, footprintH, self, selfGen))
        {
            return StepResult::BlockedUnit;
        }
    }
    outNext = next;
    return StepResult::Stepped;
}

void Arrive(Unit &unit, Mover &mover, cc::Vec2 where)
{
    unit.position = cc::ToRaylib(where);
    unit.velocity = { 0.0f, 0.0f };
    mover.hasMoveOrder = false;
    mover.hasPath = false;
    mover.path.clear();
    mover.pathNext = 0;
    unit.state = UnitState::Idle;
    mover.blockedTime = 0.0f;
    mover.blockedRepaths = 0;
    mover.speedCapPixelsPerSec = -1.0f;
}

void CancelAtBlocked(Unit &unit, Mover &mover, const TileMap &map)
{
    unit.velocity = { 0.0f, 0.0f };
    mover.hasMoveOrder = false;
    mover.hasPath = false;
    mover.path.clear();
    mover.pathNext = 0;
    unit.state = UnitState::Idle;
    mover.blockedTime = 0.0f;
    mover.blockedRepaths = 0;
    mover.speedCapPixelsPerSec = -1.0f;
    SnapUnitToTile(unit);

    if (map.Width() > 0 && map.Height() > 0)
    {
        cc::Vec2 clamped = cc::ToGlm(unit.position);
        clamped.x =
            std::clamp(clamped.x, 0.0f, static_cast<float>(map.Width()) * cc::TILE_SIZE - 1.0f);
        clamped.y =
            std::clamp(clamped.y, 0.0f, static_cast<float>(map.Height()) * cc::TILE_SIZE - 1.0f);
        unit.position = cc::ToRaylib(cc::SnapToTile(clamped));
    }
}

constexpr float kBlockedRetryDelaySeconds = 0.5f;
constexpr int kMaxBlockedRepaths = 3;

bool TryBlockedRetryWithBudget(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                               OccupancyGrid *occ, Entity self, std::uint32_t selfGen,
                               float dtSeconds, float &blockedTime, int &blockedRepaths)
{
    unit.velocity = { 0.0f, 0.0f };
    blockedTime += dtSeconds;
    if (blockedTime < kBlockedRetryDelaySeconds)
    {
        return true;
    }
    if (blockedRepaths >= kMaxBlockedRepaths)
    {
        return false;
    }
    ++blockedRepaths;
    blockedTime = 0.0f;
    const cc::IVec2 start = cc::WorldToTile(cc::ToGlm(unit.position));
    const cc::IVec2 goal = cc::WorldToTile(cc::SnapToTile(cc::ToGlm(mover.moveTarget)));
    TilePath fresh;
    if (occ != nullptr)
    {
        fresh = FindPathFootprint(map, *occ, start, goal, unit.footprintWidth,
                                  unit.footprintHeight, self, selfGen);
    }
    else
    {
        fresh = FindPath(map, start, goal);
    }
    if (fresh.empty())
    {
        return true;
    }
    if (fresh.size() == 1)
    {
        Arrive(unit, mover, cc::ToGlm(mover.moveTarget));
        OnOrderFinished(unit, orders, mover, map, occ, self, selfGen);
        return true;
    }
    mover.path = std::move(fresh);
    mover.pathNext = 1;
    mover.hasPath = true;
    mover.hasMoveOrder = true;
    return true;
}

bool TryBlockedRetry(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                     OccupancyGrid *occ, Entity self, std::uint32_t selfGen, float dtSeconds)
{
    return TryBlockedRetryWithBudget(unit, orders, mover, map, occ, self, selfGen, dtSeconds,
                                     mover.blockedTime, mover.blockedRepaths);
}

struct TileHash
{
    std::size_t operator()(cc::IVec2 tile) const
    {
        return (static_cast<std::size_t>(static_cast<std::uint32_t>(tile.x)) * 73856093u) ^
               (static_cast<std::size_t>(static_cast<std::uint32_t>(tile.y)) * 19349663u);
    }
};

}

Facing FacingFromVelocity(Vector2 velocity)
{
    constexpr float kPi = 3.141592653589793f;
    float angle = std::atan2(velocity.y, velocity.x);
    if (angle < 0.0f)
    {
        angle += 2.0f * kPi;
    }
    const int octant =
        static_cast<int>((angle + kPi / 8.0f) / (kPi / 4.0f)) % 8;
    return static_cast<Facing>((6 - octant + 8) % 8);
}

void UpdateUnitMovement(Unit &unit, Orders &orders, Mover &mover, CombatState &combat,
                        const TileMap &map, float speedPixelsPerSec, float dtSeconds,
                        OccupancyGrid *occ, Entity self, std::uint32_t selfGen)
{
    if (!mover.hasMoveOrder && !mover.hasPath)
    {
        return;
    }

    const float step = speedPixelsPerSec * dtSeconds;
    unit.state = UnitState::Moving;
    combat.phase = AttackPhase::Ready;

    if (mover.hasPath)
    {
        if (mover.pathNext >= mover.path.size())
        {
            Arrive(unit, mover, cc::ToGlm(mover.moveTarget));
            OnOrderFinished(unit, orders, mover, map, occ, self, selfGen);
            return;
        }

        const cc::IVec2 node = mover.path[mover.pathNext];
        const cc::Vec2 waypoint = cc::TileToWorld(node.x, node.y);
        cc::Vec2 next = cc::ToGlm(unit.position);
        switch (StepToward(cc::ToGlm(unit.position), waypoint, step, map, next,
                           occ, self, selfGen, unit.footprintWidth, unit.footprintHeight))
        {
        case StepResult::Arrived:
            unit.position = cc::ToRaylib(waypoint);
            ++mover.pathNext;
            if (mover.pathNext >= mover.path.size())
            {
                Arrive(unit, mover, cc::ToGlm(mover.moveTarget));
                OnOrderFinished(unit, orders, mover, map, occ, self, selfGen);
            }
            else
            {
                unit.velocity = { 0.0f, 0.0f };
            }
            return;
        case StepResult::Blocked:
            CancelAtBlocked(unit, mover, map);
            OnOrderCancelled(orders);
            return;
        case StepResult::BlockedUnit:
            if (TryBlockedRetry(unit, orders, mover, map, occ, self, selfGen, dtSeconds))
            {
                return;
            }
            CancelAtBlocked(unit, mover, map);
            OnOrderCancelled(orders);
            return;
        case StepResult::Stepped:
            mover.blockedTime = 0.0f;
            unit.velocity = cc::ToRaylib((waypoint - cc::ToGlm(unit.position)) /
                                         glm::length(waypoint - cc::ToGlm(unit.position)) * speedPixelsPerSec);
            unit.facing = FacingFromVelocity(unit.velocity);
            unit.position = cc::ToRaylib(next);
            return;
        }
    }

    cc::Vec2 next = cc::ToGlm(unit.position);
    switch (StepToward(cc::ToGlm(unit.position), cc::ToGlm(mover.moveTarget), step, map, next,
                       occ, self, selfGen, unit.footprintWidth, unit.footprintHeight))
    {
    case StepResult::Arrived:
        Arrive(unit, mover, cc::ToGlm(mover.moveTarget));
        OnOrderFinished(unit, orders, mover, map, occ, self, selfGen);
        return;
    case StepResult::Blocked:
        CancelAtBlocked(unit, mover, map);
        OnOrderCancelled(orders);
        return;
    case StepResult::BlockedUnit:
        if (TryBlockedRetry(unit, orders, mover, map, occ, self, selfGen, dtSeconds))
        {
            return;
        }
        CancelAtBlocked(unit, mover, map);
        OnOrderCancelled(orders);
        return;
    case StepResult::Stepped:
        mover.blockedTime = 0.0f;
        unit.velocity = cc::ToRaylib((cc::ToGlm(mover.moveTarget) - cc::ToGlm(unit.position)) /
                                     glm::length(cc::ToGlm(mover.moveTarget) - cc::ToGlm(unit.position)) *
                                     speedPixelsPerSec);
        unit.facing = FacingFromVelocity(unit.velocity);
        unit.position = cc::ToRaylib(next);
        return;
    }
}

void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ)
{
    std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> byTile;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.health > 0.0f && !IsEmbarked(registry, id))
        {
            byTile[cc::WorldToTile(cc::ToGlm(unit.position))].push_back(id);
        }
    });

    std::vector<cc::IVec2> claimedThisFrame;

    for (auto &[tile, occupants] : byTile)
    {
        if (occupants.size() < 2)
        {
            continue;
        }
        if (!occ.InBounds(tile))
        {
            continue;
        }
        const OccEntry holder = occ.GetUnit(tile);
        bool allIdle = true;
        bool anyDeadlockedNow = false;
        Entity idleNonHolderPick = kInvalidEntity;
        Entity idleFallbackPick = kInvalidEntity;
        Entity deadlockedNonHolderPick = kInvalidEntity;
        Entity anyNonHolderPick = kInvalidEntity;
        for (Entity id : occupants)
        {
            const Unit *unit = registry.Get<Unit>(id);
            if (unit == nullptr)
            {
                continue;
            }
            const bool isHolder =
                holder.entity == id && holder.generation == registry.Generation(id);
            const Mover *mover = FindMover(registry, id);
            const bool hasOrder =
                mover != nullptr && (mover->hasMoveOrder || mover->hasPath);
            const bool isDeadlockedNow =
                hasOrder && mover->blockedRepaths >= kMaxBlockedRepaths && mover->blockedTime > 0.0f;
            if (hasOrder)
            {
                allIdle = false;
                if (isDeadlockedNow)
                {
                    anyDeadlockedNow = true;
                }
            }
            if (!isHolder && (anyNonHolderPick == kInvalidEntity || id < anyNonHolderPick))
            {
                anyNonHolderPick = id;
            }
            if (!hasOrder)
            {
                if (idleFallbackPick == kInvalidEntity || id < idleFallbackPick)
                {
                    idleFallbackPick = id;
                }
                if (!isHolder && (idleNonHolderPick == kInvalidEntity || id < idleNonHolderPick))
                {
                    idleNonHolderPick = id;
                }
            }
            if (isDeadlockedNow && !isHolder &&
                (deadlockedNonHolderPick == kInvalidEntity || id < deadlockedNonHolderPick))
            {
                deadlockedNonHolderPick = id;
            }
        }
        Entity pick = kInvalidEntity;
        if (allIdle)
        {
            pick = (idleNonHolderPick != kInvalidEntity) ? idleNonHolderPick : idleFallbackPick;
        }
        else if (anyDeadlockedNow)
        {
            pick = (deadlockedNonHolderPick != kInvalidEntity) ? deadlockedNonHolderPick
                                                               : anyNonHolderPick;
        }
        if (pick == kInvalidEntity)
        {
            continue;
        }

        Unit *unit = registry.Get<Unit>(pick);
        cc::IVec2 dest = NearestEnterableTile(map, occ, tile, unit->footprintWidth,
                                              unit->footprintHeight, pick,
                                              registry.Generation(pick));
        for (int guard = 0; guard < 8 &&
                            std::find(claimedThisFrame.begin(), claimedThisFrame.end(), dest) !=
                                claimedThisFrame.end();
             ++guard)
        {
            dest = NearestEnterableTile(map, occ, dest + cc::IVec2(1, 0), unit->footprintWidth,
                                        unit->footprintHeight, pick,
                                        registry.Generation(pick));
        }
        claimedThisFrame.push_back(dest);

        IssuePathOrderFootprint(*unit, GetOrders(registry, pick), GetMover(registry, pick), map,
                                occ, cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)), pick,
                                registry.Generation(pick));
        // The stacked start tile blocks the footprint pathfinder (the neighbor
        // still owns it), so fall back to a straight move order: step-out walks
        // the unit clear and repaths from the freed tile.
        if (FindMover(registry, pick)->path.empty())
        {
            IssueMoveOrder(*unit, GetOrders(registry, pick), GetMover(registry, pick),
                           cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)));
        }
    }
}
