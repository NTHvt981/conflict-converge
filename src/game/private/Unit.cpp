#include "Unit.h"

#include "MathUtils.h" // glm integration check: game TU exercises cc::Vec2 conversions.
#include "Pathfinder.h" // M3 Goal 5: chase orders route around blocked tiles.
#include "Targeting.h"  // M3 Goal 5: acquire/validate targets, range checks.
#include "TileMap.h"   // M2 Goal 4: movement stops at blocked tiles.

// Stub: unit behavior, AI, and factory arrive in M3.

void IssueMoveOrder(Unit &unit, Vector2 worldTarget)
{
    unit.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    unit.hasMoveOrder = true;
}

namespace
{

// Fire raw attackPower and restart the cooldown (M4 routes through the matrix).
void FireAt(Unit &attacker, Unit &target)
{
    target.health -= static_cast<float>(attacker.attackPower);
    attacker.cooldown = attacker.cooldownTime;
}

void StopMoving(Unit &unit)
{
    unit.hasMoveOrder = false;
    unit.hasPath = false;
    unit.path.clear();
    unit.pathNext = 0;
    unit.velocity = { 0.0f, 0.0f };
}

} // namespace

void UpdateUnit(Entity self, Registry &registry, const TileMap &map, float dtSeconds)
{
    Unit *unit = registry.Get<Unit>(self);
    if (unit == nullptr || unit->health <= 0.0f)
    {
        return; // missing, or dead awaiting factory teardown (M3G6)
    }

    if (unit->cooldown > 0.0f)
    {
        unit->cooldown -= dtSeconds;
        if (unit->cooldown < 0.0f)
        {
            unit->cooldown = 0.0f;
        }
    }

    // Explicit player orders win over acquiring NEW targets — but a unit
    // already engaging (chase path with a set target) stops to fire the
    // moment its target enters range instead of walking past it.
    if (unit->hasMoveOrder || unit->hasPath)
    {
        if (unit->target != kInvalidEntity)
        {
            Unit *target = registry.Get<Unit>(unit->target);
            if (target == nullptr || target->health <= 0.0f || target->teamID == unit->teamID)
            {
                unit->target = kInvalidEntity;
            }
            else if (InAttackRange(*unit, *target))
            {
                StopMoving(*unit);
                unit->state = UnitState::Attacking;
                if (unit->cooldown <= 0.0f && unit->attackPower > 0)
                {
                    FireAt(*unit, *target);
                }
                return;
            }
        }
        UpdateUnitMovement(*unit, map, unit->speed, dtSeconds);
        return;
    }

    // Drop stale targets (destroyed, already dead, or no longer hostile).
    if (unit->target != kInvalidEntity)
    {
        const Unit *target = registry.Get<Unit>(unit->target);
        if (target == nullptr || target->health <= 0.0f || target->teamID == unit->teamID)
        {
            unit->target = kInvalidEntity;
        }
    }
    if (unit->target == kInvalidEntity)
    {
        unit->target = AcquireTarget(registry, self);
    }
    if (unit->target == kInvalidEntity)
    {
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    const Unit *target = registry.Get<Unit>(unit->target);
    if (InAttackRange(*unit, *target))
    {
        unit->state = UnitState::Attacking;
        unit->velocity = { 0.0f, 0.0f };
        if (unit->cooldown <= 0.0f && unit->attackPower > 0)
        {
            // M3 fires raw power; M4 routes this through the damage matrix.
            FireAt(*unit, *registry.Get<Unit>(unit->target));
        }
        return;
    }

    // Chase: re-path only when the target entered a new tile, then walk.
    // An unreachable target degrades to an M2 straight-line bump (IssuePathOrder fallback).
    const cc::IVec2 targetTile = cc::WorldToTile(cc::ToGlm(target->position));
    if (!unit->hasPath || !(cc::WorldToTile(cc::ToGlm(unit->moveTarget)) == targetTile))
    {
        IssuePathOrder(*unit, map, target->position);
    }
    UpdateUnitMovement(*unit, map, unit->speed, dtSeconds);
}

namespace
{

enum class StepResult
{
    Arrived, // within one step: caller snaps to target
    Blocked, // next position enters a blocked tile: caller cancels + snaps
    Stepped, // advanced one step toward the target
};

// Advance pos toward target by at most step; reports (not applies) arrival.
StepResult StepToward(cc::Vec2 pos, cc::Vec2 target, float step, const TileMap &map, cc::Vec2 &outNext)
{
    const cc::Vec2 diff = target - pos;
    const float dist = glm::length(diff);
    if (dist <= step)
    {
        outNext = target;
        return StepResult::Arrived;
    }

    const cc::Vec2 next = pos + diff / dist * step;
    if (map.IsBlocked(cc::WorldToTile(next)))
    {
        return StepResult::Blocked;
    }
    outNext = next;
    return StepResult::Stepped;
}

// Shared stop states so path and straight-line arrivals match M2 behavior.
void Arrive(Unit &unit, cc::Vec2 where)
{
    unit.position = cc::ToRaylib(where);
    unit.velocity = { 0.0f, 0.0f };
    unit.hasMoveOrder = false;
    unit.hasPath = false;
    unit.path.clear();
    unit.pathNext = 0;
    unit.state = UnitState::Idle;
}

void CancelAtBlocked(Unit &unit)
{
    unit.velocity = { 0.0f, 0.0f };
    unit.hasMoveOrder = false;
    unit.hasPath = false;
    unit.path.clear();
    unit.pathNext = 0;
    unit.state = UnitState::Idle;
    SnapUnitToTile(unit);
}

} // namespace

void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds)
{
    if (!unit.hasMoveOrder && !unit.hasPath)
    {
        return;
    }

    const float step = speedPixelsPerSec * dtSeconds;
    unit.state = UnitState::Moving;

    // M3 Goal 3: walk the A* waypoints (tile top-left corners, so every
    // stop stays snapped). A consumed path (cursor past the end, e.g. a
    // start == goal order) arrives immediately at the snapped moveTarget.
    if (unit.hasPath)
    {
        if (unit.pathNext >= unit.path.size())
        {
            Arrive(unit, cc::ToGlm(unit.moveTarget));
            return;
        }

        const cc::IVec2 node = unit.path[unit.pathNext];
        const cc::Vec2 waypoint = cc::TileToWorld(node.x, node.y);
        cc::Vec2 next = cc::ToGlm(unit.position);
        switch (StepToward(cc::ToGlm(unit.position), waypoint, step, map, next))
        {
        case StepResult::Arrived:
            unit.position = cc::ToRaylib(waypoint);
            ++unit.pathNext;
            if (unit.pathNext >= unit.path.size())
            {
                Arrive(unit, cc::ToGlm(unit.moveTarget));
            }
            else
            {
                unit.velocity = { 0.0f, 0.0f };
            }
            return;
        case StepResult::Blocked:
            // Paths avoid blocked tiles by construction; a block here means
            // the map changed mid-walk, so cancel rather than push through.
            CancelAtBlocked(unit);
            return;
        case StepResult::Stepped:
            unit.velocity = cc::ToRaylib((waypoint - cc::ToGlm(unit.position)) /
                                         glm::length(waypoint - cc::ToGlm(unit.position)) * speedPixelsPerSec);
            unit.position = cc::ToRaylib(next);
            return;
        }
    }

    // M2 straight-line fallback (no path, or path exhausted its order).
    cc::Vec2 next = cc::ToGlm(unit.position);
    switch (StepToward(cc::ToGlm(unit.position), cc::ToGlm(unit.moveTarget), step, map, next))
    {
    case StepResult::Arrived:
        // Arrival: land exactly on the snapped destination.
        Arrive(unit, cc::ToGlm(unit.moveTarget));
        return;
    case StepResult::Blocked:
        // Blocked: hold position, snapped, order cancelled.
        CancelAtBlocked(unit);
        return;
    case StepResult::Stepped:
        unit.velocity = cc::ToRaylib((cc::ToGlm(unit.moveTarget) - cc::ToGlm(unit.position)) /
                                     glm::length(cc::ToGlm(unit.moveTarget) - cc::ToGlm(unit.position)) *
                                     speedPixelsPerSec);
        unit.position = cc::ToRaylib(next);
        return;
    }
}
