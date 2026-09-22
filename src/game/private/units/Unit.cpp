#include "Unit.h"

#include "Combat.h"
#include "FogOfWar.h"
#include "MathUtils.h"
#include "Pathfinder.h"
#include "Targeting.h"
#include "TileMap.h"
#include "Building.h"
#include "UnitStats.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "UnitStats.h"

void IssueMoveOrder(Unit &unit, Vector2 worldTarget)
{
    unit.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    unit.hasMoveOrder = true;
    unit.blockedTime = 0.0f;
    unit.blockedRepaths = 0;
    ClearOrders(unit);
}

void IssueAttackMoveOrder(Unit &unit, const TileMap &map, Vector2 worldTarget)
{
    ClearOrders(unit);
    IssuePathOrder(unit, map, worldTarget);
    unit.attackMove = true;
    unit.attackMoveDest = unit.moveTarget;
}

void IssueAttackMoveOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                   Vector2 worldTarget, Entity self, std::uint32_t selfGen)
{
    ClearOrders(unit);
    IssuePathOrderFootprint(unit, map, occ, worldTarget, self, selfGen);
    unit.attackMove = true;
    unit.attackMoveDest = unit.moveTarget;
}

namespace
{

void LoseTarget(Unit &unit);

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
        LoseTarget(unit);
    }
}

void IssuePatrolOrder(Unit &unit, const TileMap &map, Vector2 pointA, Vector2 pointB)
{
    ClearOrders(unit);
    unit.stance = Stance::Patrol;
    unit.hasPatrol = true;
    unit.patrolA = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointA)));
    unit.patrolB = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointB)));
    unit.patrolToB = true;
    IssuePathOrder(unit, map, unit.patrolB);
}

namespace
{

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
    unit.phase = AttackPhase::Ready;
    unit.phaseTime = 0.0f;
}

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

void IssueRepairOrder(Unit &engineer, Entity target)
{
    if (engineer.type != UnitType::Engineer)
    {
        return;
    }
    StopMoving(engineer);
    ClearOrders(engineer);
    engineer.hasRepairOrder = true;
    engineer.repairTarget = target;
}

namespace
{

void DispatchQueuedOrder(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                         std::uint32_t selfGen, const QueuedOrder &order)
{
    ClearOrders(unit);
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
        if (occ != nullptr)
        {
            IssueAttackGroundOrderFootprint(unit, map, *occ, order.pointA, self, selfGen);
        }
        else
        {
            IssueAttackGroundOrder(unit, map, order.pointA);
        }
        break;
    case QueuedOrderKind::Count:
        break;
    }
}

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

void OnOrderCancelled(Unit &unit)
{
    unit.orderQueue.clear();
}

}

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

bool LostToFog(const Unit &unit, const Unit &target, const FogOfWar *fog)
{
    return fog != nullptr && unit.type != UnitType::Artillery &&
           !fog->IsVisible(unit.teamID, cc::WorldToTile(cc::ToGlm(target.position)));
}

constexpr float kRepairRange = 128.0f;
constexpr float kRepairRate = 15.0f;

bool IsRepairableUnit(const Unit &unit)
{
    return unit.type == UnitType::IFV || unit.type == UnitType::Artillery ||
           unit.type == UnitType::LightTank || unit.type == UnitType::HeavyTank;
}

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

void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, int teamID,
                                 std::vector<Entity> &out)
{
    std::vector<Entity> units;
    QueryUnitsInRect(registry, worldArea, teamID, units);
    for (const Entity id : units)
    {
        const Unit *unit = registry.Get<Unit>(id);
        if (unit != nullptr && IsRepairableUnit(*unit) &&
            unit->health < BaseStats(unit->type).health)
        {
            out.push_back(id);
        }
    }
    std::vector<Entity> buildings;
    QueryBuildingsInRect(registry, worldArea, teamID, buildings);
    for (const Entity id : buildings)
    {
        const Building *building = registry.Get<Building>(id);
        if (building != nullptr && building->state == BuildingState::Operational &&
            building->health < building->maxHealth)
        {
            out.push_back(id);
        }
    }
}

namespace
{

Vector2 RepairCandidatePos(const Registry &registry, Entity candidate)
{
    if (const Unit *unit = registry.Get<Unit>(candidate))
    {
        return unit->position;
    }
    if (const Building *building = registry.Get<Building>(candidate))
    {
        return BuildingCenter(*building);
    }
    return { 0.0f, 0.0f };
}

}

int AssignAreaRepair(const Registry &registry, const std::vector<Entity> &engineers,
                     const std::vector<Entity> &candidates,
                     std::vector<RepairAssignment> &out)
{
    std::vector<bool> claimed(candidates.size(), false);
    int assigned = 0;
    for (const Entity engineerId : engineers)
    {
        const Unit *engineer = registry.Get<Unit>(engineerId);
        if (engineer == nullptr)
        {
            continue;
        }
        float bestDistSq = -1.0f;
        std::size_t bestIndex = 0;
        bool found = false;
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            if (claimed[i] || !CanRepairTarget(registry, *engineer, candidates[i]))
            {
                continue;
            }
            const Vector2 goal = RepairCandidatePos(registry, candidates[i]);
            const float dx = goal.x - engineer->position.x;
            const float dy = goal.y - engineer->position.y;
            const float distSq = dx * dx + dy * dy;
            if (!found || distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestIndex = i;
                found = true;
            }
        }
        if (found)
        {
            claimed[bestIndex] = true;
            out.push_back({ engineerId, candidates[bestIndex] });
            ++assigned;
        }
    }
    return assigned;
}

void ClearOrders(Unit &unit)
{
    unit.attackMove = false;
    unit.hasPatrol = false;
    unit.hasRepairOrder = false;
    unit.repairTarget = kInvalidEntity;
    unit.hasAttackGroundOrder = false;
    unit.speedCapPixelsPerSec = -1.0f;
}

void IssueAttackGroundOrder(Unit &unit, const TileMap &map, Vector2 worldPos)
{
    ClearOrders(unit);
    unit.hasAttackGroundOrder = true;
    unit.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
    IssuePathOrder(unit, map, unit.attackGroundPos);
}

void IssueAttackGroundOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                     Vector2 worldPos, Entity self, std::uint32_t selfGen)
{
    ClearOrders(unit);
    unit.hasAttackGroundOrder = true;
    unit.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
    IssuePathOrderFootprint(unit, map, occ, unit.attackGroundPos, self, selfGen);
}

Vector2 ResolvePlayerRetreatHome(Registry &registry, Vector2 rallyPos)
{
    if (rallyPos.x != 0.0f || rallyPos.y != 0.0f)
    {
        return rallyPos;
    }
    Vector2 armyCentroid = { 0.0f, 0.0f };
    int aliveCount = 0;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == 0 && unit.health > 0.0f)
        {
            armyCentroid.x += unit.position.x;
            armyCentroid.y += unit.position.y;
            ++aliveCount;
        }
    });
    if (aliveCount > 0)
    {
        armyCentroid.x /= static_cast<float>(aliveCount);
        armyCentroid.y /= static_cast<float>(aliveCount);
    }

    Vector2 home = { 0.0f, 0.0f };
    float bestDistSq = -1.0f;
    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID != 0 || building.type != BuildingType::Base ||
            building.state != BuildingState::Operational)
        {
            return;
        }
        const cc::Vec2 corner = cc::TileToWorld(building.tileX, building.tileY);
        const Vector2 center = { corner.x + 32.0f, corner.y + 32.0f };
        const float dx = center.x - armyCentroid.x;
        const float dy = center.y - armyCentroid.y;
        const float distSq = dx * dx + dy * dy;
        if (bestDistSq < 0.0f || distSq < bestDistSq)
        {
            bestDistSq = distSq;
            home = center;
        }
    });
    return home;
}

void RetreatIfLowHP(Registry &registry, TileMap &map, OccupancyGrid *occ, Vector2 home, int teamID,
                    float healthFraction, bool onlyAutoRetreat)
{
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.teamID != teamID || unit.health <= 0.0f || unit.type == UnitType::Engineer)
        {
            return;
        }
        if (onlyAutoRetreat && !unit.autoRetreat)
        {
            return;
        }
        const float maxHealth = BaseStats(unit.type).health;
        if (maxHealth <= 0.0f || unit.health >= healthFraction * maxHealth)
        {
            return;
        }
        const cc::IVec2 wantTile = cc::WorldToTile(cc::ToGlm(home));
        const cc::IVec2 goalTile =
            (occ != nullptr)
                ? NearestEnterableTile(map, *occ, wantTile, unit.footprintWidth,
                                       unit.footprintHeight, id, registry.Generation(id))
                : wantTile;
        if (unit.attackMove && unit.hasPath &&
            cc::WorldToTile(cc::ToGlm(unit.moveTarget)) == goalTile)
        {
            return;
        }
        if (occ != nullptr)
        {
            IssueAttackMoveOrderFootprint(unit, map, *occ, home, id, registry.Generation(id));
        }
        else
        {
            IssueAttackMoveOrder(unit, map, home);
        }
    });
}

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
                DemolishBuilding(registry, map, id);
            }
        });
        return true;
    }
    return false;
}

void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
                const FogOfWar *fog, OccupancyGrid *occ,
                const std::unordered_map<Entity, float> *reserved)
{
    Unit *unit = registry.Get<Unit>(self);
    if (unit == nullptr || unit->health <= 0.0f)
    {
        return;
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
            UpdateUnitMovement(*unit, map, EffectiveSpeed(*unit), dtSeconds, occ, self, registry.Generation(self));
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

    if (unit->hasAttackGroundOrder)
    {
        const float dist =
            glm::distance(cc::ToGlm(unit->position), cc::ToGlm(unit->attackGroundPos));
        if (dist > static_cast<float>(unit->attackRange))
        {
            ReissueDriverOrder(*unit, map, occ, unit->attackGroundPos, self, registry);
            UpdateUnitMovement(*unit, map, EffectiveSpeed(*unit), dtSeconds, occ, self,
                               registry.Generation(self));
            unit->state = UnitState::Moving;
            return;
        }
        unit->state = UnitState::Attacking;
        unit->velocity = { 0.0f, 0.0f };
        UpdateAttackPhases(*unit, dtSeconds, [&] {
            ResolveGroundAttack(registry, *unit, unit->attackGroundPos);
        });
        return;
    }

    if (unit->hasMoveOrder || unit->hasPath)
    {
        if (unit->attackMove)
        {
            unit->target = AcquireTarget(registry, self, fog, reserved);
            if (unit->target == kInvalidEntity)
            {
                unit->target = AcquireBuildingTarget(registry, self, fog);
            }
            if (unit->target != kInvalidEntity &&
                ValidateTarget(registry, *unit, unit->target, fog))
            {
                if (EngageTarget(*unit, registry, map, unit->target, fog, dtSeconds))
                {
                    return;
                }
                if (occ != nullptr)
                {
                    IssuePathOrderFootprint(*unit, map, *occ,
                                            TargetPosition(registry, unit->target), self,
                                            registry.Generation(self));
                }
                else
                {
                    IssuePathOrder(*unit, map, TargetPosition(registry, unit->target));
                }
                UpdateUnitMovement(*unit, map, EffectiveSpeed(*unit), dtSeconds, occ, self, registry.Generation(self));
                unit->state = UnitState::Moving;
                return;
            }
            unit->target = kInvalidEntity;
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
        UpdateUnitMovement(*unit, map, EffectiveSpeed(*unit), dtSeconds, occ, self, registry.Generation(self));
        return;
    }

    if (unit->target != kInvalidEntity && !ValidateTarget(registry, *unit, unit->target, fog))
    {
        LoseTarget(*unit);
    }
    if (unit->target == kInvalidEntity)
    {
        unit->target = AcquireTarget(registry, self, fog, reserved);
        if (unit->target != kInvalidEntity && unit->stance == Stance::Hold)
        {
            const Unit *sighting = registry.Get<Unit>(unit->target);
            if (sighting == nullptr || !InAttackRange(*unit, *sighting))
            {
                LoseTarget(*unit);
            }
        }
    }
    if (unit->target == kInvalidEntity && unit->stance != Stance::Hold)
    {
        unit->target = AcquireBuildingTarget(registry, self, fog);
    }
    if (unit->target == kInvalidEntity)
    {
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
        LoseTarget(*unit);
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    ReissueDriverOrder(*unit, map, occ, TargetPosition(registry, unit->target), self, registry);
    UpdateUnitMovement(*unit, map, EffectiveSpeed(*unit), dtSeconds, occ, self, registry.Generation(self));
}

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
    unit.speedCapPixelsPerSec = -1.0f;
}

void CancelAtBlocked(Unit &unit, const TileMap &map)
{
    unit.velocity = { 0.0f, 0.0f };
    unit.hasMoveOrder = false;
    unit.hasPath = false;
    unit.path.clear();
    unit.pathNext = 0;
    unit.state = UnitState::Idle;
    unit.blockedTime = 0.0f;
    unit.blockedRepaths = 0;
    unit.speedCapPixelsPerSec = -1.0f;
    SnapUnitToTile(unit);

    // Make sure unit dont drift off-map
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

bool TryBlockedRetryWithBudget(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                               std::uint32_t selfGen, float dtSeconds, float &blockedTime,
                               int &blockedRepaths)
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
        return true;
    }
    if (fresh.size() == 1)
    {
        Arrive(unit, cc::ToGlm(unit.moveTarget));
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

}

void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds,
                        OccupancyGrid *occ, Entity self, std::uint32_t selfGen)
{
    if (!unit.hasMoveOrder && !unit.hasPath)
    {
        return;
    }

    const float step = speedPixelsPerSec * dtSeconds;
    unit.state = UnitState::Moving;
    unit.phase = AttackPhase::Ready;

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
            CancelAtBlocked(unit, map);
            OnOrderCancelled(unit);
            return;
        case StepResult::BlockedUnit:
            if (TryBlockedRetry(unit, map, occ, self, selfGen, dtSeconds))
            {
                return;
            }
            CancelAtBlocked(unit, map);
            OnOrderCancelled(unit);
            return;
        case StepResult::Stepped:
            unit.blockedTime = 0.0f;
            unit.velocity = cc::ToRaylib((waypoint - cc::ToGlm(unit.position)) /
                                         glm::length(waypoint - cc::ToGlm(unit.position)) * speedPixelsPerSec);
            unit.facing = FacingFromVelocity(unit.velocity);
            unit.position = cc::ToRaylib(next);
            return;
        }
    }

    cc::Vec2 next = cc::ToGlm(unit.position);
    switch (StepToward(cc::ToGlm(unit.position), cc::ToGlm(unit.moveTarget), step, map, next,
                       occ, self, selfGen, unit.footprintWidth, unit.footprintHeight))
    {
    case StepResult::Arrived:
        Arrive(unit, cc::ToGlm(unit.moveTarget));
        OnOrderFinished(unit, map, occ, self, selfGen);
        return;
    case StepResult::Blocked:
        CancelAtBlocked(unit, map);
        OnOrderCancelled(unit);
        return;
    case StepResult::BlockedUnit:
        if (TryBlockedRetry(unit, map, occ, self, selfGen, dtSeconds))
        {
            return;
        }
        CancelAtBlocked(unit, map);
        OnOrderCancelled(unit);
        return;
    case StepResult::Stepped:
        unit.blockedTime = 0.0f;
        unit.velocity = cc::ToRaylib((cc::ToGlm(unit.moveTarget) - cc::ToGlm(unit.position)) /
                                     glm::length(cc::ToGlm(unit.moveTarget) - cc::ToGlm(unit.position)) *
                                     speedPixelsPerSec);
        unit.facing = FacingFromVelocity(unit.velocity);
        unit.position = cc::ToRaylib(next);
        return;
    }
}

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
}

void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ)
{
    std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> byTile;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.health > 0.0f)
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
            const bool hasOrder = unit->hasMoveOrder || unit->hasPath;
            const bool isDeadlockedNow =
                hasOrder && unit->blockedRepaths >= kMaxBlockedRepaths && unit->blockedTime > 0.0f;
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

        IssuePathOrderFootprint(*unit, map, occ, cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)),
                               pick, registry.Generation(pick));
        // The stacked start tile blocks the footprint pathfinder (the neighbor
        // still owns it), so fall back to a straight move order: step-out walks
        // the unit clear and repaths from the freed tile.
        if (unit->path.empty())
        {
            IssueMoveOrder(*unit, cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)));
        }
    }
}

void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
                          const FogOfWar *fog, float dtSeconds)
{
	occ.ReleaseAllUnitFootprints();

    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.health > 0.0f)
        {
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));

            (void)occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                            registry.Generation(id));
        }
    });

    ReservedDamageMap reservedDamage;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.health > 0.0f && unit.target != kInvalidEntity &&
            (unit.phase == AttackPhase::WindUp || unit.phase == AttackPhase::Recover))
        {
            reservedDamage[unit.target] += static_cast<float>(unit.attackPower);
        }
    });

    registry.Each<Unit>([&](Entity id, Unit &) {
        UpdateUnit(id, registry, map, dtSeconds, fog, &occ, &reservedDamage);
    });

    ResolveStackedUnits(registry, map, occ);
}
