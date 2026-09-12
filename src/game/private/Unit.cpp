#include "Unit.h"

#include "MathUtils.h" // glm integration check: game TU exercises cc::Vec2 conversions.
#include "TileMap.h"   // M2 Goal 4: movement stops at blocked tiles.

// Stub: unit behavior, AI, and factory arrive in M3.

void IssueMoveOrder(Unit &unit, Vector2 worldTarget)
{
    unit.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    unit.hasMoveOrder = true;
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
