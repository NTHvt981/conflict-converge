#include "Unit.h"

#include "Combat.h"     // M4 Goal 1: FireAt routes through the damage matrix.
#include "FogOfWar.h"   // M9: gate acquisition + chase validation on visibility.
#include "MathUtils.h" // glm integration check: game TU exercises cc::Vec2 conversions.
#include "Pathfinder.h" // M3 Goal 5: chase orders route around blocked tiles.
#include "Targeting.h"  // M3 Goal 5: acquire/validate targets, range checks.
#include "TileMap.h"   // M2 Goal 4: movement stops at blocked tiles.
#include "Building.h"  // M13: repair targets include structures.
#include "UnitStats.h" // M13: max-health lookup for repair validation.

// Stub: unit behavior, AI, and factory arrive in M3.

#include "UnitStats.h" // M13: max-health lookup for repair validation.

void IssueMoveOrder(Unit &unit, Vector2 worldTarget)
{
    unit.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    unit.hasMoveOrder = true;
    // A plain move replaces fancier orders (attack-move, repair).
    unit.attackMove = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
}

void IssueAttackMoveOrder(Unit &unit, const TileMap &map, Vector2 worldTarget)
{
    unit.attackMove = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
    IssuePathOrder(unit, map, worldTarget); // A* (or straight fallback)
    unit.attackMove = true;
    unit.attackMoveDest = unit.moveTarget;
}

void SetStance(Unit &unit, Stance stance)
{
    unit.stance = stance;
    if (stance != Stance::Patrol)
    {
        unit.hasPatrol = false;
    }
    if (stance == Stance::Hold)
    {
        // Stand down immediately; firing in range resumes below.
        unit.target = kInvalidEntity;
    }
}

void IssuePatrolOrder(Unit &unit, const TileMap &map, Vector2 pointA, Vector2 pointB)
{
    unit.stance = Stance::Patrol;
    unit.hasPatrol = true;
    unit.patrolA = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointA)));
    unit.patrolB = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointB)));
    unit.patrolToB = true;
    unit.attackMove = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
    IssuePathOrder(unit, map, unit.patrolB);
}

namespace
{

// M4 Goal 3: strike phasing. Ready + cooled + armed -> WindUp; WindUp expiry
// lands the hit via landHit and enters Recover; Recover ends when the
// cooldown hits zero. Called only while in range of a valid target.
// M13: templated on the landing blow so structures share the machine.
template <typename LandHit> void UpdateAttackPhases(Unit &attacker, float dtSeconds, LandHit landHit)
{
    if (attacker.phase == AttackPhase::Ready)
    {
        if (attacker.cooldown <= 0.0f && attacker.attackPower > 0)
        {
            attacker.phase = AttackPhase::WindUp;
            attacker.phaseTime = attacker.windupTime;
        }
        return;
    }
    if (attacker.phase == AttackPhase::WindUp)
    {
        attacker.phaseTime -= dtSeconds;
        if (attacker.phaseTime <= 0.0f)
        {
            landHit();
            attacker.phase = AttackPhase::Recover;
        }
        return;
    }
    if (attacker.cooldown <= 0.0f)
    {
        attacker.phase = AttackPhase::Ready;
    }
}

void UpdateAttack(Unit &attacker, Unit &target, float dtSeconds)
{
    UpdateAttackPhases(attacker, dtSeconds, [&] { ResolveAttack(attacker, target); });
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

void IssueRepairOrder(Unit &engineer, Entity target)
{
    if (engineer.type != UnitType::Engineer)
    {
        return;
    }
    StopMoving(engineer);
    engineer.attackMove = false;
    engineer.hasRepairOrder = true;
    engineer.repairTarget = target;
}

// M9: a set target standing on a tile the unit's team cannot see is dropped —
// except for Artillery, which blind-fires into shroud at no penalty (Q78).
bool LostToFog(const Unit &unit, const Unit &target, const FogOfWar *fog)
{
    return fog != nullptr && unit.type != UnitType::Artillery &&
           !fog->IsVisible(unit.teamID, cc::WorldToTile(cc::ToGlm(target.position)));
}

// M13: repair tuning. Channel rate is HP/sec; cost is time only (Q83).
constexpr float kRepairRange = 128.0f;
constexpr float kRepairRate = 15.0f;

// Only mechanical units take wrenches; infantry flesh is left alone.
bool IsRepairableUnit(const Unit &unit)
{
    return unit.type == UnitType::IFV || unit.type == UnitType::Artillery ||
           unit.type == UnitType::LightTank || unit.type == UnitType::HeavyTank;
}

// Validate a repair order and report where to work. False (order dies) for
// non-Engineers, dead/foreign/healthy targets, flesh units, and wrecked
// (non-Operational) buildings.
bool RepairAim(const Registry &registry, const Unit &engineer, Entity target, Vector2 &outPos)
{
    if (engineer.type != UnitType::Engineer)
    {
        return false;
    }
    if (const Unit *u = registry.Get<Unit>(target))
    {
        if (u->health <= 0.0f || u->teamID != engineer.teamID || !IsRepairableUnit(*u))
        {
            return false;
        }
        if (u->health >= BaseStats(u->type).health)
        {
            return false;
        }
        outPos = u->position;
        return true;
    }
    if (const Building *b = registry.Get<Building>(target))
    {
        if (b->state != BuildingState::Operational || b->teamID != engineer.teamID)
        {
            return false;
        }
        if (b->health >= b->maxHealth)
        {
            return false;
        }
        outPos = BuildingCenter(*b);
        return true;
    }
    return false;
}

// Approach tile for repair work: the aim tile itself when walkable (units),
// else the nearest passable ring (building footprints are blocked, so the
// engineer parks beside the structure instead of pushing into it).
cc::IVec2 RepairApproachTile(const TileMap &map, cc::IVec2 aimTile)
{
    if (!map.InBounds(aimTile))
    {
        return aimTile;
    }
    if (!map.IsBlocked(aimTile))
    {
        return aimTile;
    }
    for (int ring = 1; ring <= 3; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (dx * dx + dy * dy > ring * ring)
                {
                    continue;
                }
                const cc::IVec2 tile{ aimTile.x + dx, aimTile.y + dy };
                if (map.InBounds(tile) && !map.IsBlocked(tile))
                {
                    return tile;
                }
            }
        }
    }
    return aimTile;
}

// M13: polymorphic targets (units and structures share Entity IDs).
// ValidateTarget: living hostile unit or Operational hostile building,
// visible unless the seeker blind-fires (fog null or artillery).
bool ValidateTarget(Registry &registry, const Unit &seeker, Entity id, const FogOfWar *fog)
{
    const bool seesThroughFog =
        (fog == nullptr) || seeker.type == UnitType::Artillery;
    if (Unit *target = registry.Get<Unit>(id))
    {
        if (target->health <= 0.0f || target->teamID == seeker.teamID ||
            LostToFog(seeker, *target, fog))
        {
            return false;
        }
        return true;
    }
    if (Building *building = registry.Get<Building>(id))
    {
        if (building->state != BuildingState::Operational || building->teamID == seeker.teamID)
        {
            return false;
        }
        return seesThroughFog ||
               fog->IsVisible(seeker.teamID,
                              cc::WorldToTile(cc::ToGlm(BuildingCenter(*building))));
    }
    return false;
}

// Aim point for either kind: unit position or structure center.
Vector2 TargetPosition(Registry &registry, Entity id)
{
    if (Unit *target = registry.Get<Unit>(id))
    {
        return target->position;
    }
    if (Building *building = registry.Get<Building>(id))
    {
        return BuildingCenter(*building);
    }
    return { 0.0f, 0.0f };
}

// Fire when a validated target is in range (phase machine + demolish at
// zero HP). Sets Attacking state. True only when a shot cycle ran —
// out-of-range or invalid targets return false for the caller to chase.
bool EngageTarget(Unit &attacker, Registry &registry, TileMap &map, Entity id,
                  const FogOfWar *fog, float dtSeconds)
{
    if (!ValidateTarget(registry, attacker, id, fog))
    {
        return false;
    }
    if (Unit *target = registry.Get<Unit>(id))
    {
        if (!InAttackRange(attacker, *target))
        {
            return false;
        }
        attacker.state = UnitState::Attacking;
        attacker.velocity = { 0.0f, 0.0f };
        UpdateAttack(attacker, *target, dtSeconds);
        return true;
    }
    if (Building *building = registry.Get<Building>(id))
    {
        const Vector2 center = BuildingCenter(*building);
        if (glm::distance(cc::ToGlm(attacker.position), cc::ToGlm(center)) >
            static_cast<float>(attacker.attackRange))
        {
            return false;
        }
        attacker.state = UnitState::Attacking;
        attacker.velocity = { 0.0f, 0.0f };
        UpdateAttackPhases(attacker, dtSeconds, [&] {
            ResolveBuildingAttack(attacker, *building);
            if (building->health <= 0.0f)
            {
                // Safe mid-iteration: structures live in their own pool.
                DemolishBuilding(registry, map, id);
            }
        });
        return true;
    }
    return false;
}

void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
                const FogOfWar *fog)
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
    if (unit->hitFlashTime > 0.0f)
    {
        unit->hitFlashTime -= dtSeconds;
        if (unit->hitFlashTime < 0.0f)
        {
            unit->hitFlashTime = 0.0f;
        }
    }

    // M13: repair orders behave like move orders with a job at the end.
    // Approach out-of-range targets (re-path on tile change, chase-style),
    // channel HP inside 96px, drop the order when there is nothing to fix.
    if (unit->hasRepairOrder)
    {
        Vector2 aim = {};
        if (!RepairAim(registry, *unit, unit->repairTarget, aim))
        {
            unit->hasRepairOrder = false;
            unit->repairTarget = kInvalidEntity;
        }
        else if (glm::distance(cc::ToGlm(unit->position), cc::ToGlm(aim)) > kRepairRange)
        {
            const cc::IVec2 goalTile = RepairApproachTile(map, cc::WorldToTile(cc::ToGlm(aim)));
            if (!unit->hasPath || !(cc::WorldToTile(cc::ToGlm(unit->moveTarget)) == goalTile))
            {
                IssuePathOrder(*unit, map, cc::ToRaylib(cc::TileToWorld(goalTile.x, goalTile.y)));
            }
            UpdateUnitMovement(*unit, map, unit->speed, dtSeconds);
            unit->state = UnitState::Moving;
            return;
        }
        else
        {
            const float step = kRepairRate * dtSeconds;
            if (Unit *patient = registry.Get<Unit>(unit->repairTarget))
            {
                const float max = BaseStats(patient->type).health;
                patient->health = patient->health + step >= max ? max : patient->health + step;
            }
            else if (Building *site = registry.Get<Building>(unit->repairTarget))
            {
                site->health =
                    site->health + step >= site->maxHealth ? site->maxHealth : site->health + step;
            }
            unit->state = UnitState::Idle;
            unit->velocity = { 0.0f, 0.0f };
            return;
        }
    }

    // Explicit player orders win over acquiring NEW targets — but a unit
    // already engaging (chase path with a set target) stops to fire the
    // moment its target enters range instead of walking past it.
    if (unit->hasMoveOrder || unit->hasPath)
    {
        // M13: attack-move scans on the march. Contact -> engage in place
        // (orders intact); contact lost -> resume the recorded destination.
        // Structures are contact too: marches raze production on the way.
        if (unit->attackMove)
        {
            unit->target = AcquireTarget(registry, self, fog);
            if (unit->target == kInvalidEntity)
            {
                unit->target = AcquireBuildingTarget(registry, self, fog);
            }
            if (unit->target != kInvalidEntity &&
                ValidateTarget(registry, *unit, unit->target, fog))
            {
                if (EngageTarget(*unit, registry, map, unit->target, fog, dtSeconds))
                {
                    return; // orders intact: the march resumes after the kill
                }
                IssuePathOrder(*unit, map, TargetPosition(registry, unit->target)); // detour
                UpdateUnitMovement(*unit, map, unit->speed, dtSeconds);
                unit->state = UnitState::Moving;
                return;
            }
            unit->target = kInvalidEntity;
            const cc::IVec2 destTile = cc::WorldToTile(cc::ToGlm(unit->attackMoveDest));
            if (!unit->hasMoveOrder || !unit->hasPath ||
                !(cc::WorldToTile(cc::ToGlm(unit->moveTarget)) == destTile))
            {
                IssuePathOrder(*unit, map, unit->attackMoveDest); // (re)march
            }
        }
        if (unit->target != kInvalidEntity)
        {
            if (!ValidateTarget(registry, *unit, unit->target, fog))
            {
                unit->target = kInvalidEntity;
            }
            else if (EngageTarget(*unit, registry, map, unit->target, fog, dtSeconds))
            {
                StopMoving(*unit);
                return;
            }
        }
        UpdateUnitMovement(*unit, map, unit->speed, dtSeconds);
        return;
    }

    // Drop stale targets of either kind (destroyed, dead/wrecked, friendly,
    // or fog-hidden for non-artillery).
    if (unit->target != kInvalidEntity && !ValidateTarget(registry, *unit, unit->target, fog))
    {
        unit->target = kInvalidEntity;
    }
    if (unit->target == kInvalidEntity)
    {
        unit->target = AcquireTarget(registry, self, fog);
        if (unit->target != kInvalidEntity && unit->stance == Stance::Hold)
        {
            // Hold: stand still, firing only at what is already in range.
            const Unit *sighting = registry.Get<Unit>(unit->target);
            if (sighting == nullptr || !InAttackRange(*unit, *sighting))
            {
                unit->target = kInvalidEntity;
            }
        }
    }
    if (unit->target == kInvalidEntity && unit->stance != Stance::Hold)
    {
        // No troops to fight: raze nearby hostile structures instead.
        unit->target = AcquireBuildingTarget(registry, self, fog);
    }
    if (unit->target == kInvalidEntity)
    {
        // M13: patrol loops its legs while idle with no combat to answer.
        if (unit->stance == Stance::Patrol && unit->hasPatrol && !unit->hasMoveOrder &&
            !unit->hasPath)
        {
            const Vector2 leg = unit->patrolToB ? unit->patrolB : unit->patrolA;
            unit->patrolToB = !unit->patrolToB;
            IssuePathOrder(*unit, map, leg);
            unit->state = UnitState::Moving;
            return;
        }
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    if (EngageTarget(*unit, registry, map, unit->target, fog, dtSeconds))
    {
        return;
    }
    if (unit->stance == Stance::Hold)
    {
        // Out of range and holding: stand down instead of chasing.
        unit->target = kInvalidEntity;
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    // Chase: re-path only when the target entered a new tile, then walk.
    // An unreachable target degrades to an M2 straight-line bump (IssuePathOrder fallback).
    const Vector2 aimPos = TargetPosition(registry, unit->target);
    const cc::IVec2 targetTile = cc::WorldToTile(cc::ToGlm(aimPos));
    if (!unit->hasPath || !(cc::WorldToTile(cc::ToGlm(unit->moveTarget)) == targetTile))
    {
        IssuePathOrder(*unit, map, aimPos);
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
    // M4 Goal 3: stepping cancels any telegraph — a mover never lands a hit.
    unit.phase = AttackPhase::Ready;

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
