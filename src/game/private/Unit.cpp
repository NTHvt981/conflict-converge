#include "Unit.h"

#include "MathUtils.h" // glm integration check: game TU exercises cc::Vec2 conversions.
#include "TileMap.h"   // M2 Goal 4: movement stops at blocked tiles.

// Stub: unit behavior, AI, and factory arrive in M3.

void IssueMoveOrder(Unit &unit, Vector2 worldTarget)
{
    unit.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    unit.hasMoveOrder = true;
}

void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds)
{
    if (!unit.hasMoveOrder)
    {
        return;
    }

    const cc::Vec2 pos = cc::ToGlm(unit.position);
    const cc::Vec2 target = cc::ToGlm(unit.moveTarget);
    const cc::Vec2 diff = target - pos;
    const float dist = glm::length(diff);
    const float step = speedPixelsPerSec * dtSeconds;

    unit.state = UnitState::Moving;
    if (dist <= step)
    {
        // Arrival: land exactly on the snapped destination.
        unit.position = unit.moveTarget;
        unit.velocity = { 0.0f, 0.0f };
        unit.hasMoveOrder = false;
        unit.state = UnitState::Idle;
        return;
    }

    const cc::Vec2 dir = diff / dist;
    const cc::Vec2 next = pos + dir * step;
    if (map.IsBlocked(cc::WorldToTile(next)))
    {
        // Blocked: hold position, snapped, order cancelled.
        unit.velocity = { 0.0f, 0.0f };
        unit.hasMoveOrder = false;
        unit.state = UnitState::Idle;
        SnapUnitToTile(unit);
        return;
    }

    unit.velocity = cc::ToRaylib(dir * speedPixelsPerSec);
    unit.position = cc::ToRaylib(next);
}
