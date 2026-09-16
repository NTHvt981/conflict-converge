#include "Unit.h"

#include "Combat.h"     // M4 Goal 1: FireAt routes through the damage matrix.
#include "FogOfWar.h"   // M9: gate acquisition + chase validation on visibility.
#include "MathUtils.h" // glm integration check: game TU exercises cc::Vec2 conversions.
#include "Pathfinder.h" // M3 Goal 5: chase orders route around blocked tiles.
#include "Targeting.h"  // M3 Goal 5: acquire/validate targets, range checks.
#include "TileMap.h"   // M2 Goal 4: movement stops at blocked tiles.
#include "Building.h"  // M13: repair targets include structures.
#include "UnitStats.h" // M13: max-health lookup for repair validation.

#include <utility> // std::pair: RunUnitMovementFrame's pre-move position snapshot

// Stub: unit behavior, AI, and factory arrive in M3.

#include "UnitStats.h" // M13: max-health lookup for repair validation.

void IssueMoveOrder(Unit &unit, Vector2 worldTarget)
{
    unit.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    unit.hasMoveOrder = true;
    unit.blockedTime = 0.0f;
    unit.blockedRepaths = 0;
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

void IssueAttackMoveOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                   Vector2 worldTarget, Entity self, std::uint32_t selfGen)
{
    unit.attackMove = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
    IssuePathOrderFootprint(unit, map, occ, worldTarget, self, selfGen);
    unit.attackMove = true;
    unit.attackMoveDest = unit.moveTarget;
}

namespace
{

void LoseTarget(Unit &unit); // defined beside StopMoving below

// Driver-issued leg (chase, remarch, repair approach, patrol): re-issue only
// when no path is active or the sanitized goal tile changed — this bounds
// footprint A* to tile changes instead of every frame while chasing (the
// old detour re-pathed unconditionally). Footprint-aware when occ is
// present, legacy blind otherwise. Returns true when an order was issued.
bool ReissueDriverOrder(Unit &unit, TileMap &map, OccupancyGrid *occ, Vector2 dest,
                        Entity self, Registry &registry)
{
    const cc::IVec2 wantTile = cc::WorldToTile(cc::ToGlm(dest));
    const cc::IVec2 goalTile =
        (occ != nullptr) ? NearestEnterableTile(map, *occ, wantTile, unit.footprintWidth,
                                                unit.footprintHeight, self,
                                                registry.Generation(self))
                         : wantTile;
    if (unit.hasPath && cc::WorldToTile(cc::ToGlm(unit.moveTarget)) == goalTile)
    {
        return false;
    }
    if (occ != nullptr)
    {
        IssuePathOrderFootprint(unit, map, *occ, dest, self, registry.Generation(self));
    }
    else
    {
        IssuePathOrder(unit, map, dest);
    }
    return true;
}

} // namespace

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
        LoseTarget(unit);
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

void LoseTarget(Unit &unit)
{
    unit.target = kInvalidEntity;
    // Dropping a target cancels any telegraph: the next engagement must run
    // the full windup. Otherwise a mid-WindUp unit whose target dies carries
    // residual phaseTime into a freshly acquired target and lands early,
    // bypassing part of the intended telegraph window.
    unit.phase = AttackPhase::Ready;
    unit.phaseTime = 0.0f;
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

namespace
{

// Dispatches one queued order through the same Issue* functions as live
// orders. Clears every other order's fields first so exactly one is active
// (queued dispatch always starts clean, unlike some live paths that only
// clear a subset). AttackGround has no order type yet — ignored until it
// lands (enqueue sites must not produce it before then).
void DispatchQueuedOrder(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                         std::uint32_t selfGen, const QueuedOrder &order)
{
    unit.attackMove = false;
    unit.hasPatrol = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
    switch (order.kind)
    {
    case QueuedOrderKind::Move:
        if (occ != nullptr)
        {
            IssuePathOrderFootprint(unit, map, *occ, order.pointA, self, selfGen);
        }
        else
        {
            IssuePathOrder(unit, map, order.pointA);
        }
        break;
    case QueuedOrderKind::AttackMove:
        if (occ != nullptr)
        {
            IssueAttackMoveOrderFootprint(unit, map, *occ, order.pointA, self, selfGen);
        }
        else
        {
            IssueAttackMoveOrder(unit, map, order.pointA);
        }
        break;
    case QueuedOrderKind::Patrol:
        IssuePatrolOrder(unit, map, order.pointA, order.pointB);
        break;
    case QueuedOrderKind::Repair:
        IssueRepairOrder(unit, order.target);
        break;
    case QueuedOrderKind::AttackGround:
    case QueuedOrderKind::Count:
        break;
    }
}

// Runs the next queued order after the current one genuinely finishes
// (arrival, repair-target lost). Skipped for patrol (loops forever —
// anything queued behind one runs only if the patrol is overwritten) and
// when the queue is empty. One dispatch per call, so chains terminate.
void OnOrderFinished(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                     std::uint32_t selfGen)
{
    if (unit.hasPatrol || unit.orderQueue.empty())
    {
        return;
    }
    const QueuedOrder next = unit.orderQueue.front();
    unit.orderQueue.erase(unit.orderQueue.begin());
    DispatchQueuedOrder(unit, map, occ, self, selfGen, next);
}

// A cancelled order (blocked-budget exhausted) drops the rest of the queue
// too: a stuck unit blindly marching into queued orders it also can't reach
// is worse UX than stopping for a new player command.
void OnOrderCancelled(Unit &unit)
{
    unit.orderQueue.clear();
}

} // namespace

void IssueOrEnqueue(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                    std::uint32_t selfGen, bool shiftQueue, QueuedOrder order)
{
    if (!shiftQueue)
    {
        unit.orderQueue.clear();
        DispatchQueuedOrder(unit, map, occ, self, selfGen, order);
        return;
    }
    if (!unit.hasMoveOrder && !unit.hasPath && !unit.hasRepairOrder && !unit.hasPatrol &&
        unit.orderQueue.empty())
    {
        DispatchQueuedOrder(unit, map, occ, self, selfGen, order);
        return;
    }
    unit.orderQueue.push_back(order);
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

bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target)
{
    Vector2 aim = {};
    return RepairAim(registry, engineer, target, aim);
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
    // Rings 1..6 (not 1..3): wide obstructions (large footprints, rubble
    // fields, lake edges) need the extra reach before giving up, and the
    // scan is a few hundred tile checks on repair orders only.
    for (int ring = 1; ring <= 6; ++ring)
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
                const FogOfWar *fog, OccupancyGrid *occ)
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
            OnOrderFinished(*unit, map, occ, self, registry.Generation(self));
        }
        else if (glm::distance(cc::ToGlm(unit->position), cc::ToGlm(aim)) > kRepairRange)
        {
            const cc::IVec2 goalTile = RepairApproachTile(map, cc::WorldToTile(cc::ToGlm(aim)));
            ReissueDriverOrder(*unit, map, occ,
                               cc::ToRaylib(cc::TileToWorld(goalTile.x, goalTile.y)), self,
                               registry);
            UpdateUnitMovement(*unit, map, unit->speed, dtSeconds, occ, self, registry.Generation(self));
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
                // Chase detour: re-issued every frame while closing (NOT tile-
                // guarded like the other legs). A guard here makes blocked
                // chasers stand and wait on the retry budget while the melee
                // flows around them; the churn-and-replan keeps them sliding
                // into contact, and the M-vs-H soak is tuned on exactly that
                // (a tile guard flipped it). Footprint-aware when bound.
                if (occ != nullptr)
                {
                    IssuePathOrderFootprint(*unit, map, *occ,
                                            TargetPosition(registry, unit->target), self,
                                            registry.Generation(self));
                }
                else
                {
                    IssuePathOrder(*unit, map, TargetPosition(registry, unit->target)); // detour
                }
                UpdateUnitMovement(*unit, map, unit->speed, dtSeconds, occ, self, registry.Generation(self));
                unit->state = UnitState::Moving;
                return;
            }
            unit->target = kInvalidEntity;
            // (re)march: resumes the recorded destination (re-sanitized: the
            // tile may have filled since the order was issued).
            ReissueDriverOrder(*unit, map, occ, unit->attackMoveDest, self, registry);
        }
        if (unit->target != kInvalidEntity)
        {
            if (!ValidateTarget(registry, *unit, unit->target, fog))
            {
                LoseTarget(*unit);
            }
            else if (EngageTarget(*unit, registry, map, unit->target, fog, dtSeconds))
            {
                StopMoving(*unit);
                return;
            }
        }
        UpdateUnitMovement(*unit, map, unit->speed, dtSeconds, occ, self, registry.Generation(self));
        return;
    }

    // Drop stale targets of either kind (destroyed, dead/wrecked, friendly,
    // or fog-hidden for non-artillery).
    if (unit->target != kInvalidEntity && !ValidateTarget(registry, *unit, unit->target, fog))
    {
        LoseTarget(*unit);
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
                LoseTarget(*unit);
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
            ReissueDriverOrder(*unit, map, occ, leg, self, registry);
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
        LoseTarget(*unit);
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    // Chase: re-path only when the target entered a new tile, then walk.
    // An unreachable target degrades to an M2 straight-line bump (IssuePathOrder fallback).
    ReissueDriverOrder(*unit, map, occ, TargetPosition(registry, unit->target), self, registry);
    UpdateUnitMovement(*unit, map, unit->speed, dtSeconds, occ, self, registry.Generation(self));
}

namespace
{

enum class StepResult
{
    Arrived,    // within one step: caller snaps to target
    Blocked,    // next position enters terrain-blocked tile: caller cancels
    BlockedUnit,// next position enters a unit-occupied tile: caller retries
    Stepped,    // advanced one step toward the target
};

// Advance pos toward target by at most step; reports (not applies) arrival.
// Phase 4: when occ is non-null, checks footprint occupancy to prevent
// stepping into tiles occupied by other entities.
StepResult StepToward(cc::Vec2 pos, cc::Vec2 target, float step, const TileMap &map, cc::Vec2 &outNext,
                      OccupancyGrid *occ = nullptr, Entity self = 0, std::uint32_t selfGen = 0,
                      int footprintW = 1, int footprintH = 1)
{
    if (step <= 0.0f)
    {
        // No movement budget (degenerate dt): hold position without
        // touching outNext. BlockedUnit waits and retries rather than
        // cancelling (terrain) or snapping (arrival), and — unlike the
        // dist <= step check below — it can't divide by zero when a
        // negative step meets dist == 0.
        return StepResult::BlockedUnit;
    }
    const cc::Vec2 diff = target - pos;
    const float dist = glm::length(diff);
    if (dist <= step)
    {
        outNext = target;
        return StepResult::Arrived;
    }

    const cc::Vec2 next = pos + diff / dist * step;
    const cc::IVec2 from = cc::WorldToTile(pos);
    const cc::IVec2 to = cc::WorldToTile(next);
    // Blocked only when stepping INTO a blocked tile from open ground. A
    // unit caught on a blocked tile (spawned inside a fresh footprint) may
    // always step out — otherwise the first step cancels the order and the
    // unit is trapped forever.
    if (map.IsBlocked(to) && to != from)
    {
        return StepResult::Blocked;
    }
    // Phase 4: occupancy check — reject moves into tiles occupied by other
    // entities (full footprint check for multi-tile units). Transient by
    // nature (units move), so the caller waits and replans instead of
    // cancelling like it does for permanent terrain blocks.
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
    unit.blockedTime = 0.0f;
    unit.blockedRepaths = 0;
    unit.separationStallTime = 0.0f;
    unit.separationStallRepaths = 0;
}

void CancelAtBlocked(Unit &unit)
{
    unit.velocity = { 0.0f, 0.0f };
    unit.hasMoveOrder = false;
    unit.hasPath = false;
    unit.path.clear();
    unit.pathNext = 0;
    unit.state = UnitState::Idle;
    unit.blockedTime = 0.0f;
    unit.blockedRepaths = 0;
    unit.separationStallTime = 0.0f;
    unit.separationStallRepaths = 0;
    SnapUnitToTile(unit);
}

// Blocked-move retry: a transient unit blocker shouldn't kill the order.
// Holds position while blockedTime accrues; every retry interval the route
// to moveTarget is replanned (footprint-aware when occ is present).
// Returns true when the order survives (waiting, arrived via replan, or
// replanned), false when the repath budget is spent and the caller should
// cancel as before.
constexpr float kBlockedRetryDelaySeconds = 0.5f;
constexpr int kMaxBlockedRepaths = 3;

// Shared by both blockedTime/blockedRepaths (StepResult::BlockedUnit) and
// separationStallTime/separationStallRepaths (ReportSeparationStall) so the
// two failure modes get the same wait/replan/cancel behavior without
// sharing a counter (see the field comment on Unit::separationStallTime).
bool TryBlockedRetryWithBudget(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                               std::uint32_t selfGen, float dtSeconds, float &blockedTime,
                               int &blockedRepaths)
{
    unit.velocity = { 0.0f, 0.0f };
    blockedTime += dtSeconds;
    if (blockedTime < kBlockedRetryDelaySeconds)
    {
        return true; // hold position, keep the order
    }
    if (blockedRepaths >= kMaxBlockedRepaths)
    {
        return false; // budget spent: caller cancels
    }
    ++blockedRepaths;
    blockedTime = 0.0f;
    const cc::IVec2 start = cc::WorldToTile(cc::ToGlm(unit.position));
    const cc::IVec2 goal = cc::WorldToTile(cc::SnapToTile(cc::ToGlm(unit.moveTarget)));
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
        return true; // no route yet: keep waiting on the old waypoints
    }
    if (fresh.size() == 1)
    {
        Arrive(unit, cc::ToGlm(unit.moveTarget)); // replanned onto our own tile
        OnOrderFinished(unit, map, occ, self, selfGen);
        return true;
    }
    unit.path = std::move(fresh);
    unit.pathNext = 1;
    unit.hasPath = true;
    unit.hasMoveOrder = true;
    return true;
}

bool TryBlockedRetry(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                     std::uint32_t selfGen, float dtSeconds)
{
    return TryBlockedRetryWithBudget(unit, map, occ, self, selfGen, dtSeconds, unit.blockedTime,
                                     unit.blockedRepaths);
}

} // namespace

void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds,
                        OccupancyGrid *occ, Entity self, std::uint32_t selfGen)
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
            OnOrderFinished(unit, map, occ, self, selfGen);
            return;
        }

        const cc::IVec2 node = unit.path[unit.pathNext];
        const cc::Vec2 waypoint = cc::TileToWorld(node.x, node.y);
        cc::Vec2 next = cc::ToGlm(unit.position);
        switch (StepToward(cc::ToGlm(unit.position), waypoint, step, map, next,
                           occ, self, selfGen, unit.footprintWidth, unit.footprintHeight))
        {
        case StepResult::Arrived:
            unit.position = cc::ToRaylib(waypoint);
            ++unit.pathNext;
            if (unit.pathNext >= unit.path.size())
            {
                Arrive(unit, cc::ToGlm(unit.moveTarget));
                OnOrderFinished(unit, map, occ, self, selfGen);
            }
            else
            {
                unit.velocity = { 0.0f, 0.0f };
            }
            return;
        case StepResult::Blocked:
            // Terrain blocks are permanent: cancel immediately (M2 legacy).
            CancelAtBlocked(unit);
            OnOrderCancelled(unit);
            return;
        case StepResult::BlockedUnit:
            // Paths avoid occupied tiles by construction; a block here means
            // a unit crossed mid-walk, so wait and replan a few times before
            // cancelling rather than dying on the first transient contact.
            if (TryBlockedRetry(unit, map, occ, self, selfGen, dtSeconds))
            {
                return;
            }
            CancelAtBlocked(unit);
            OnOrderCancelled(unit);
            return;
        case StepResult::Stepped:
            unit.blockedTime = 0.0f; // progress: not stuck
            unit.velocity = cc::ToRaylib((waypoint - cc::ToGlm(unit.position)) /
                                         glm::length(waypoint - cc::ToGlm(unit.position)) * speedPixelsPerSec);
            unit.position = cc::ToRaylib(next);
            return;
        }
    }

    // M2 straight-line fallback (no path, or path exhausted its order).
    cc::Vec2 next = cc::ToGlm(unit.position);
    switch (StepToward(cc::ToGlm(unit.position), cc::ToGlm(unit.moveTarget), step, map, next,
                       occ, self, selfGen, unit.footprintWidth, unit.footprintHeight))
    {
    case StepResult::Arrived:
        // Arrival: land exactly on the snapped destination.
        Arrive(unit, cc::ToGlm(unit.moveTarget));
        OnOrderFinished(unit, map, occ, self, selfGen);
        return;
    case StepResult::Blocked:
        // Terrain: hold position, snapped, order cancelled (M2 legacy).
        CancelAtBlocked(unit);
        OnOrderCancelled(unit);
        return;
    case StepResult::BlockedUnit:
        // Units: wait and replan first; only cancel when the budget runs
        // out (see the path branch above).
        if (TryBlockedRetry(unit, map, occ, self, selfGen, dtSeconds))
        {
            return;
        }
        CancelAtBlocked(unit);
        OnOrderCancelled(unit);
        return;
    case StepResult::Stepped:
        unit.blockedTime = 0.0f; // progress: not stuck
        unit.velocity = cc::ToRaylib((cc::ToGlm(unit.moveTarget) - cc::ToGlm(unit.position)) /
                                     glm::length(cc::ToGlm(unit.moveTarget) - cc::ToGlm(unit.position)) *
                                     speedPixelsPerSec);
        unit.position = cc::ToRaylib(next);
        return;
    }
}

void SeparateUnits(Registry &registry, float dtSeconds)
{
    // Body half-extent scales with the footprint (16px per tile): 1x1 keeps
    // the legacy 32px body (M4G2 hitbox), 2x2 vehicles push as 64px bodies
    // so crowds of mixed sizes relax instead of interpenetrating. The
    // per-pair push is capped so crowds relax over frames, not teleport.
    constexpr float kPushPerSecond = 96.0f;
    if (dtSeconds <= 0.0f)
    {
        return;
    }
    struct Item
    {
        Unit *unit = nullptr;
        float half = 16.0f;
    };
    std::vector<Item> items;
    registry.Each<Unit>([&](Entity, Unit &unit) {
        if (unit.health > 0.0f)
        {
            const int dim = unit.footprintWidth > unit.footprintHeight ? unit.footprintWidth
                                                                       : unit.footprintHeight;
            items.push_back({ &unit, 16.0f * static_cast<float>(dim > 0 ? dim : 1) });
        }
    });
    const float cap = kPushPerSecond * dtSeconds;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        for (std::size_t j = i + 1; j < items.size(); ++j)
        {
            // Body spans [pos+16, pos+16+2*half]; center accordingly.
            const cc::Vec2 a = cc::ToGlm(items[i].unit->position) +
                               cc::Vec2(16.0f + items[i].half, 16.0f + items[i].half);
            const cc::Vec2 b = cc::ToGlm(items[j].unit->position) +
                               cc::Vec2(16.0f + items[j].half, 16.0f + items[j].half);
            const cc::Vec2 delta = a - b;
            const float dist = glm::length(delta);
            const float minDist = items[i].half + items[j].half;
            if (dist >= minDist)
            {
                continue;
            }
            // Exact stacks split along +x (deterministic, no RNG).
            const cc::Vec2 dir = dist > 0.001f ? delta / dist : cc::Vec2(1.0f, 0.0f);
            float push = (minDist - dist) / 2.0f;
            if (push > cap)
            {
                push = cap;
            }
            items[i].unit->position =
                cc::ToRaylib(cc::ToGlm(items[i].unit->position) + dir * push);
            items[j].unit->position =
                cc::ToRaylib(cc::ToGlm(items[j].unit->position) - dir * push);
            // M4G4 overrun: enemies in body contact trade crush hits.
            // Crush is vehicle contact (AttackContext docs): only hulls
            // attempt it — foot-vs-foot stays bloodless as before, and
            // vehicle-on-foot is negated inside ResolveAttack (the
            // documented rule, previously dead code: no production path
            // passed Crush). Gated on each attacker's cooldown so contact
            // can't machine-gun outside the fire cycle. Proven soak-neutral
            // (identical Medium-vs-Hard trajectory with and without).
            if (items[i].unit->teamID != items[j].unit->teamID)
            {
                if (items[i].unit->health > 0.0f && IsVehicleHull(items[i].unit->type) &&
                    items[i].unit->cooldown <= 0.0f)
                {
                    ResolveAttack(*items[i].unit, *items[j].unit, AttackContext::Crush);
                }
                if (items[j].unit->health > 0.0f && IsVehicleHull(items[j].unit->type) &&
                    items[j].unit->cooldown <= 0.0f)
                {
                    ResolveAttack(*items[j].unit, *items[i].unit, AttackContext::Crush);
                }
            }
        }
    }
}

void ReportSeparationStall(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                           std::uint32_t selfGen, float dtSeconds)
{
    if (!TryBlockedRetryWithBudget(unit, map, occ, self, selfGen, dtSeconds,
                                   unit.separationStallTime, unit.separationStallRepaths))
    {
        CancelAtBlocked(unit);
        OnOrderCancelled(unit);
    }
}

void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
                          const FogOfWar *fog, float dtSeconds)
{
    // Phase 4: reserve each unit's current anchor tile before movement, so
    // StepToward's CanEnter check prevents two units from entering the same
    // tile. Ownership-checked: a shoved unit never wipes or steals another
    // unit's reservation, it just goes unreserved until separation pushes it
    // clear. Snapshot pre-move positions in the same pass for the
    // separation-stall check below.
    std::vector<std::pair<Entity, Vector2>> preMovePositions;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.health > 0.0f)
        {
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
            occ.ReleaseFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                      registry.Generation(id));
            occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                      registry.Generation(id));
        }
        preMovePositions.push_back({ id, unit.position });
    });

    registry.Each<Unit>(
        [&](Entity id, Unit &) { UpdateUnit(id, registry, map, dtSeconds, fog, &occ); });

    // Overlap avoidance: fan out stacked bodies after the AI driver.
    SeparateUnits(registry, dtSeconds);

    // Separation-stall detection: a unit whose StepToward call this frame
    // reported real forward progress (Stepped, non-zero velocity) but whose
    // net displacement for the whole frame came out near zero was pushed
    // back by SeparateUnits before it ever crossed a tile boundary. That
    // failure mode never touches CanEnter, so TryBlockedRetry's normal
    // trigger (StepResult::BlockedUnit) never fires on its own.
    constexpr float kStallProgressFraction = 0.1f;
    for (const auto &[id, before] : preMovePositions)
    {
        Unit *unit = registry.Get<Unit>(id);
        if (unit == nullptr || unit->health <= 0.0f)
        {
            continue;
        }
        if (!unit->hasMoveOrder && !unit->hasPath)
        {
            continue;
        }
        if (unit->velocity.x == 0.0f && unit->velocity.y == 0.0f)
        {
            continue; // arrived this frame, or already caught by the normal block path
        }
        const float moved = glm::length(cc::ToGlm(unit->position) - cc::ToGlm(before));
        const float expectedStep = unit->speed * dtSeconds;
        if (expectedStep > 0.0f && moved < expectedStep * kStallProgressFraction)
        {
            ReportSeparationStall(*unit, map, &occ, id, registry.Generation(id), dtSeconds);
        }
        else
        {
            // Real progress went through this frame: forgive any partial
            // stall budget instead of letting it carry over indefinitely.
            unit->separationStallTime = 0.0f;
            unit->separationStallRepaths = 0;
        }
    }
}
