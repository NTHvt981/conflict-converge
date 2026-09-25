#include "Unit.h"

#include "Combat.h"
#include "Extensions.h"
#include "FogOfWar.h"
#include "MapFile.h"
#include "MathUtils.h"
#include "Pathfinder.h"
#include "Targeting.h"
#include "TileMap.h"
#include "Building.h"
#include "UnitConfig.h"
#include "UnitStats.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "UnitStats.h"

void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 worldTarget)
{
    mover.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    mover.hasMoveOrder = true;
    mover.blockedTime = 0.0f;
    mover.blockedRepaths = 0;
    ClearOrders(unit, orders, mover);
}

void IssueAttackMoveOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                          Vector2 worldTarget)
{
    ClearOrders(unit, orders, mover);
    IssuePathOrder(unit, orders, mover, map, worldTarget);
    orders.attackMove = true;
    orders.attackMoveDest = mover.moveTarget;
}

void IssueAttackMoveOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                                   const OccupancyGrid &occ, Vector2 worldTarget, Entity self,
                                   std::uint32_t selfGen)
{
    ClearOrders(unit, orders, mover);
    IssuePathOrderFootprint(unit, orders, mover, map, occ, worldTarget, self, selfGen);
    orders.attackMove = true;
    orders.attackMoveDest = mover.moveTarget;
}

namespace
{

void LoseTarget(Unit &unit, CombatState &combat);

bool ReissueDriverOrder(Unit &unit, Mover &mover, TileMap &map, OccupancyGrid *occ, Vector2 dest,
                        Entity self, Registry &registry)
{
    const cc::IVec2 wantTile = cc::WorldToTile(cc::ToGlm(dest));
    const cc::IVec2 goalTile =
        (occ != nullptr) ? NearestEnterableTile(map, *occ, wantTile, unit.footprintWidth,
                                                unit.footprintHeight, self,
                                                registry.Generation(self))
                         : wantTile;
    if (mover.hasPath && cc::WorldToTile(cc::ToGlm(mover.moveTarget)) == goalTile)
    {
        return false;
    }
    if (occ != nullptr)
    {
        IssuePathOrderFootprint(unit, GetOrders(registry, self), mover, map, *occ, dest, self,
                                registry.Generation(self));
    }
    else
    {
        IssuePathOrder(unit, GetOrders(registry, self), mover, map, dest);
    }
    return true;
}

}

void SetStance(Unit &unit, Orders &orders, CombatState &combat, Stance stance)
{
    orders.stance = stance;
    if (stance != Stance::Patrol)
    {
        orders.hasPatrol = false;
    }
    if (stance == Stance::Hold)
    {
        LoseTarget(unit, combat);
    }
}

void IssuePatrolOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map, Vector2 pointA,
                      Vector2 pointB)
{
    ClearOrders(unit, orders, mover);
    orders.stance = Stance::Patrol;
    orders.hasPatrol = true;
    orders.patrolA = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointA)));
    orders.patrolB = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointB)));
    orders.patrolToB = true;
    IssuePathOrder(unit, orders, mover, map, orders.patrolB);
}

namespace
{

template <typename LandHit>
void UpdateAttackPhases(Unit &attacker, CombatState &combat, float dtSeconds, LandHit landHit)
{

    if (combat.phase == AttackPhase::Ready)
    {
        if (combat.cooldown <= 0.0f && attacker.attackPower > 0)
        {
            combat.phase = AttackPhase::WindUp;
            combat.phaseTime = attacker.windupTime;
        }
        return;
    }
    if (combat.phase == AttackPhase::WindUp)
    {
        combat.phaseTime -= dtSeconds;
        if (combat.phaseTime <= 0.0f)
        {
            landHit();
            combat.phase = AttackPhase::Recover;
        }
        return;
    }
    if (combat.cooldown <= 0.0f)
    {
        combat.phase = AttackPhase::Ready;
    }
}

void UpdateAttack(Entity self, Registry &registry, Unit &attacker, CombatState &combat,
                  Entity targetId, float dtSeconds)
{
    // M3 strike dispatch: WindUp ends in launch for arcing units, not damage.
    UpdateAttackPhases(attacker, combat, dtSeconds,
                       [&] { ResolveStrike(registry, self, attacker, combat, targetId); });
}

// G1 turret: radians of aim tolerance before a turreted unit may open fire.
inline constexpr float kTurretAimTolerance = 0.15f;

float WrapAngle(float angle)
{
    constexpr float kPi = 3.141592653589793f;
    while (angle > kPi)
    {
        angle -= 2.0f * kPi;
    }
    while (angle < -kPi)
    {
        angle += 2.0f * kPi;
    }
    return angle;
}

// Units without turrets always aim true.
bool TurretAimed(Registry &registry, Entity self, Vector2 fromPos, Vector2 aimPos,
                 float dtSeconds)
{
    Turret *turret = registry.Get<Turret>(self);
    if (turret == nullptr)
    {
        return true;
    }
    const float desired =
        std::atan2(aimPos.y - fromPos.y, aimPos.x - fromPos.x);
    const float diff = WrapAngle(desired - turret->facing);
    const float step = std::clamp(diff, -turret->turnRate * dtSeconds,
                                  turret->turnRate * dtSeconds);
    turret->facing = WrapAngle(turret->facing + step);
    return std::fabs(WrapAngle(desired - turret->facing)) <= kTurretAimTolerance;
}

void StopMoving(Unit &unit, Mover &mover)
{
    mover.hasMoveOrder = false;
    mover.hasPath = false;
    mover.path.clear();
    mover.pathNext = 0;
    unit.velocity = { 0.0f, 0.0f };
}

void LoseTarget(Unit &unit, CombatState &combat)
{
    (void)unit;
    combat.target = kInvalidEntity;
    combat.phase = AttackPhase::Ready;
    combat.phaseTime = 0.0f;
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

void IssueRepairOrder(Unit &engineer, Orders &orders, Mover &mover, Entity target)
{
    if (engineer.type != UnitType::Engineer)
    {
        return;
    }
    StopMoving(engineer, mover);
    ClearOrders(engineer, orders, mover);
    orders.hasRepairOrder = true;
    orders.repairTarget = target;
}

constexpr float kLoadRange = 128.0f;

bool CanLoadTarget(const Registry &registry, Entity carrier, const Unit &carrierUnit,
                   Entity passenger)
{
    if (passenger == carrier || passenger == kInvalidEntity)
    {
        return false;
    }
    const Cargo *cargo = registry.Get<Cargo>(carrier);
    if (cargo == nullptr || cargo->capacity <= 0 ||
        cargo->passengers.size() >= static_cast<std::size_t>(cargo->capacity))
    {
        return false;
    }
    const Unit *rider = registry.Get<Unit>(passenger);
    if (rider == nullptr || rider->health <= 0.0f || rider->teamID != carrierUnit.teamID)
    {
        return false;
    }
    if (!IsFleshUnit(rider->type) || IsEmbarked(registry, passenger))
    {
        return false;
    }
    return true;
}

bool BoardTransport(Registry &registry, Entity carrier, Entity passenger)
{
    Unit *carrierUnit = registry.Get<Unit>(carrier);
    Unit *rider = registry.Get<Unit>(passenger);
    if (carrierUnit == nullptr || rider == nullptr ||
        !CanLoadTarget(registry, carrier, *carrierUnit, passenger))
    {
        return false;
    }
    EmbarkedOn ride;
    ride.carrier = carrier;
    registry.Add(passenger, ride);
    registry.Get<Cargo>(carrier)->passengers.push_back(passenger);
    rider->isSelected = false;
    Mover &riderMover = GetMover(registry, passenger);
    riderMover.hasMoveOrder = false;
    riderMover.hasPath = false;
    riderMover.path.clear();
    riderMover.pathNext = 0;
    rider->velocity = { 0.0f, 0.0f };
    rider->state = UnitState::Idle;
    return true;
}

int UnloadTransport(Registry &registry, const TileMap &map, Entity carrier, Vector2 worldPos)
{
    Cargo *cargo = registry.Get<Cargo>(carrier);
    Unit *carrierUnit = registry.Get<Unit>(carrier);
    if (cargo == nullptr || carrierUnit == nullptr || cargo->passengers.empty())
    {
        return 0;
    }
    const cc::IVec2 base = cc::WorldToTile(cc::ToGlm(worldPos));
    int landed = 0;
    for (const Entity passenger : cargo->passengers)
    {
        Unit *rider = registry.Get<Unit>(passenger);
        if (rider == nullptr)
        {
            continue;
        }
        const cc::IVec2 dest = NearestFreeTile(map, base.x, base.y);
        rider->position = cc::ToRaylib(cc::TileToWorld(dest.x, dest.y));
        SnapUnitToTile(*rider);
        registry.Remove<EmbarkedOn>(passenger);
        ++landed;
    }
    cargo->passengers.clear();
    return landed;
}

void IssueLoadOrder(Unit &carrier, Orders &orders, Mover &mover, Entity passenger)
{
    StopMoving(carrier, mover);
    ClearOrders(carrier, orders, mover);
    orders.hasLoadOrder = true;
    orders.loadTarget = passenger;
}

void IssueUnloadOrder(Unit &carrier, Orders &orders, Mover &mover, const TileMap &map,
                      Vector2 worldPos)
{
    (void)map;
    StopMoving(carrier, mover);
    ClearOrders(carrier, orders, mover);
    orders.hasUnloadOrder = true;
    orders.unloadPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
}

namespace
{

void DispatchQueuedOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                         OccupancyGrid *occ, Entity self, std::uint32_t selfGen,
                         const QueuedOrder &order)
{
    ClearOrders(unit, orders, mover);
    switch (order.kind)
    {
    case QueuedOrderKind::Move:
        if (occ != nullptr)
        {
            IssuePathOrderFootprint(unit, orders, mover, map, *occ, order.pointA, self, selfGen);
        }
        else
        {
            IssuePathOrder(unit, orders, mover, map, order.pointA);
        }
        break;
    case QueuedOrderKind::AttackMove:
        if (occ != nullptr)
        {
            IssueAttackMoveOrderFootprint(unit, orders, mover, map, *occ, order.pointA, self,
                                          selfGen);
        }
        else
        {
            IssueAttackMoveOrder(unit, orders, mover, map, order.pointA);
        }
        break;
    case QueuedOrderKind::Patrol:
        IssuePatrolOrder(unit, orders, mover, map, order.pointA, order.pointB);
        break;
    case QueuedOrderKind::Repair:
        if (unit.type == UnitType::Medic)
        {
            IssueHealOrder(unit, orders, mover, order.target);
        }
        else
        {
            IssueRepairOrder(unit, orders, mover, order.target);
        }
        break;
    case QueuedOrderKind::AttackGround:
        if (occ != nullptr)
        {
            IssueAttackGroundOrderFootprint(unit, orders, mover, map, *occ, order.pointA, self,
                                            selfGen);
        }
        else
        {
            IssueAttackGroundOrder(unit, orders, mover, map, order.pointA);
        }
        break;
    case QueuedOrderKind::Load:
        IssueLoadOrder(unit, orders, mover, order.target);
        break;
    case QueuedOrderKind::Unload:
        IssueUnloadOrder(unit, orders, mover, map, order.pointA);
        break;
    case QueuedOrderKind::Count:
        break;
    }
}

void OnOrderFinished(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                     OccupancyGrid *occ, Entity self, std::uint32_t selfGen)
{
    if (orders.hasPatrol || orders.orderQueue.empty())
    {
        return;
    }
    const QueuedOrder next = orders.orderQueue.front();
    orders.orderQueue.erase(orders.orderQueue.begin());
    DispatchQueuedOrder(unit, orders, mover, map, occ, self, selfGen, next);
}

void OnOrderCancelled(Orders &orders)
{
    orders.orderQueue.clear();
}

}

void IssueOrEnqueue(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                    OccupancyGrid *occ, Entity self, std::uint32_t selfGen, bool shiftQueue,
                    QueuedOrder order)
{
    if (!shiftQueue)
    {
        orders.orderQueue.clear();
        DispatchQueuedOrder(unit, orders, mover, map, occ, self, selfGen, order);
        return;
    }
    if (!mover.hasMoveOrder && !mover.hasPath && !orders.hasRepairOrder && !orders.hasLoadOrder &&
        !orders.hasUnloadOrder && !orders.hasPatrol && orders.orderQueue.empty())
    {
        DispatchQueuedOrder(unit, orders, mover, map, occ, self, selfGen, order);
        return;
    }
    orders.orderQueue.push_back(order);
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
        if (u->health <= 0.0f || u->teamID != engineer.teamID || !IsRepairableUnit(*u) ||
            IsEmbarked(registry, target))
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

constexpr float kAutoRepairAcquireRange = 256.0f;

void AcquireAutoRepair(Registry &registry, Entity self, const Unit &engineer, Orders &orders)
{
    Entity best = kInvalidEntity;
    float bestDist = kAutoRepairAcquireRange;
    auto consider = [&](Entity candidate) {
        if (candidate == self || candidate == kInvalidEntity)
        {
            return;
        }
        Vector2 aim = {};
        if (!RepairAim(registry, engineer, candidate, aim))
        {
            return;
        }
        const float dist = glm::distance(cc::ToGlm(engineer.position), cc::ToGlm(aim));
        if (dist < bestDist)
        {
            bestDist = dist;
            best = candidate;
        }
    };
    registry.Each<Unit>([&](Entity id, const Unit &) { consider(id); });
    registry.Each<Building>([&](Entity id, const Building &) { consider(id); });
    if (best != kInvalidEntity)
    {
        orders.hasRepairOrder = true;
        orders.repairTarget = best;
    }
}

void IssueHealOrder(Unit &medic, Orders &orders, Mover &mover, Entity target)
{
    if (medic.type != UnitType::Medic)
    {
        return;
    }
    StopMoving(medic, mover);
    ClearOrders(medic, orders, mover);
    orders.hasRepairOrder = true;
    orders.repairTarget = target;
}

bool IsHealableUnit(const Unit &unit)
{
    return unit.type == UnitType::RifleInfantry || unit.type == UnitType::AntiArmorInfantry ||
           unit.type == UnitType::Engineer || unit.type == UnitType::PrototypeInfantry ||
           unit.type == UnitType::Medic;
}

bool HealAim(const Registry &registry, const Unit &medic, Entity target, Vector2 &outPos)
{
    if (medic.type != UnitType::Medic)
    {
        return false;
    }
    if (const Unit *u = registry.Get<Unit>(target))
    {
        if (u->health <= 0.0f || u->teamID != medic.teamID || !IsHealableUnit(*u) ||
            IsEmbarked(registry, target))
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
    return false;
}

bool CanHealTarget(const Registry &registry, const Unit &medic, Entity target)
{
    Vector2 aim = {};
    return HealAim(registry, medic, target, aim);
}

void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, int teamID,
                                 std::vector<Entity> &out)
{
    std::vector<Entity> units;
    QueryUnitsInRect(registry, worldArea, teamID, units);
    for (const Entity id : units)
    {
        const Unit *unit = registry.Get<Unit>(id);
        if (unit == nullptr || unit->health >= BaseStats(unit->type).health)
        {
            continue;
        }
        if (IsRepairableUnit(*unit) || IsHealableUnit(*unit))
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
            if (claimed[i] || candidates[i] == engineerId)
            {
                continue;
            }
            const bool valid = engineer->type == UnitType::Medic
                                   ? CanHealTarget(registry, *engineer, candidates[i])
                                   : CanRepairTarget(registry, *engineer, candidates[i]);
            if (!valid)
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

void ClearOrders(Unit &unit, Orders &orders, Mover &mover)
{
    (void)unit;
    orders.attackMove = false;
    orders.hasPatrol = false;
    orders.hasRepairOrder = false;
    orders.repairTarget = kInvalidEntity;
    orders.hasAttackGroundOrder = false;
    orders.hasLoadOrder = false;
    orders.loadTarget = kInvalidEntity;
    orders.hasUnloadOrder = false;
    mover.speedCapPixelsPerSec = -1.0f;
}

void IssueAttackGroundOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                            Vector2 worldPos)
{
    ClearOrders(unit, orders, mover);
    orders.hasAttackGroundOrder = true;
    orders.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
    IssuePathOrder(unit, orders, mover, map, orders.attackGroundPos);
}

void IssueAttackGroundOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                                     const OccupancyGrid &occ, Vector2 worldPos, Entity self,
                                     std::uint32_t selfGen)
{
    ClearOrders(unit, orders, mover);
    orders.hasAttackGroundOrder = true;
    orders.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
    IssuePathOrderFootprint(unit, orders, mover, map, occ, orders.attackGroundPos, self, selfGen);
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
        Orders &orders = GetOrders(registry, id);
        Mover &mover = GetMover(registry, id);
        if (onlyAutoRetreat && !orders.autoRetreat)
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
        if (orders.attackMove && mover.hasPath &&
            cc::WorldToTile(cc::ToGlm(mover.moveTarget)) == goalTile)
        {
            return;
        }
        if (occ != nullptr)
        {
            IssueAttackMoveOrderFootprint(unit, orders, mover, map, *occ, home, id,
                                          registry.Generation(id));
        }
        else
        {
            IssueAttackMoveOrder(unit, orders, mover, map, home);
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
            IsEmbarked(registry, id) || LostToFog(seeker, *target, fog))
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

bool EngageTarget(Entity self, Unit &attacker, CombatState &combat, Registry &registry,
                  TileMap &map, Entity id, const FogOfWar *fog, float dtSeconds)
{
    if (!ValidateTarget(registry, attacker, id, fog))
    {
        return false;
    }
    if (Unit *target = registry.Get<Unit>(id))
    {
        if (!InAttackRange(attacker, *target))
        {
            TurretAimed(registry, self, attacker.position, target->position, dtSeconds);
            return false;
        }
        attacker.state = UnitState::Attacking;
        attacker.velocity = { 0.0f, 0.0f };
        if (combat.phase == AttackPhase::Ready &&
            !TurretAimed(registry, self, attacker.position, target->position, dtSeconds))
        {
            return true; // traversing: visible aim, no free hits
        }
        TurretAimed(registry, self, attacker.position, target->position, dtSeconds);
        UpdateAttack(self, registry, attacker, combat, id, dtSeconds);
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
        if (combat.phase == AttackPhase::Ready &&
            !TurretAimed(registry, self, attacker.position, center, dtSeconds))
        {
            return true; // traversing: visible aim, no free hits
        }
        TurretAimed(registry, self, attacker.position, center, dtSeconds);
        UpdateAttackPhases(attacker, combat, dtSeconds, [&] {
            ResolveBuildingAttack(attacker, combat, *building);
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
    if (IsEmbarked(registry, self))
    {
        return; // inside a carrier: no cooldowns, orders, or attacks
    }
    Orders &orders = GetOrders(registry, self);
    Mover &mover = GetMover(registry, self);
    CombatState &combat = GetCombatState(registry, self);

    if (combat.cooldown > 0.0f)
    {
        combat.cooldown -= dtSeconds;
        if (combat.cooldown < 0.0f)
        {
            combat.cooldown = 0.0f;
        }
    }
    if (combat.hitFlashTime > 0.0f)
    {
        combat.hitFlashTime -= dtSeconds;
        if (combat.hitFlashTime < 0.0f)
        {
            combat.hitFlashTime = 0.0f;
        }
    }

    if (unit->type == UnitType::Engineer && orders.autoRepair && !orders.hasRepairOrder &&
        !mover.hasMoveOrder && !mover.hasPath && !orders.hasLoadOrder && !orders.hasUnloadOrder &&
        !orders.hasPatrol && orders.orderQueue.empty() && combat.target == kInvalidEntity)
    {
        AcquireAutoRepair(registry, self, *unit, orders);
    }
    if (orders.hasRepairOrder)
    {
        Vector2 aim = {};
        const bool healing = unit->type == UnitType::Medic;
        if (!(healing ? HealAim(registry, *unit, orders.repairTarget, aim)
                      : RepairAim(registry, *unit, orders.repairTarget, aim)))
        {
            orders.hasRepairOrder = false;
            orders.repairTarget = kInvalidEntity;
            OnOrderFinished(*unit, orders, mover, map, occ, self, registry.Generation(self));
        }
        else if (glm::distance(cc::ToGlm(unit->position), cc::ToGlm(aim)) > kRepairRange)
        {
            const cc::IVec2 goalTile = RepairApproachTile(map, cc::WorldToTile(cc::ToGlm(aim)));
            ReissueDriverOrder(*unit, mover, map, occ,
                               cc::ToRaylib(cc::TileToWorld(goalTile.x, goalTile.y)), self,
                               registry);
            UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self, registry.Generation(self));
            unit->state = UnitState::Moving;
            return;
        }
        else
        {
            const float step = kRepairRate * dtSeconds;
            if (Unit *patient = registry.Get<Unit>(orders.repairTarget))
            {
                const float max = BaseStats(patient->type).health;
                patient->health = patient->health + step >= max ? max : patient->health + step;
            }
            else if (Building *site = registry.Get<Building>(orders.repairTarget))
            {
                site->health =
                    site->health + step >= site->maxHealth ? site->maxHealth : site->health + step;
            }
            unit->state = UnitState::Idle;
            unit->velocity = { 0.0f, 0.0f };
            return;
        }
    }

    if (orders.hasLoadOrder)
    {
        if (!CanLoadTarget(registry, self, *unit, orders.loadTarget))
        {
            orders.hasLoadOrder = false;
            orders.loadTarget = kInvalidEntity;
            OnOrderFinished(*unit, orders, mover, map, occ, self, registry.Generation(self));
            return;
        }
        const Unit *rider = registry.Get<Unit>(orders.loadTarget);
        if (rider != nullptr &&
            glm::distance(cc::ToGlm(unit->position), cc::ToGlm(rider->position)) > kLoadRange)
        {
            ReissueDriverOrder(*unit, mover, map, occ, rider->position, self, registry);
            UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self,
                               registry.Generation(self));
            unit->state = UnitState::Moving;
            return;
        }
        BoardTransport(registry, self, orders.loadTarget);
        orders.hasLoadOrder = false;
        orders.loadTarget = kInvalidEntity;
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        OnOrderFinished(*unit, orders, mover, map, occ, self, registry.Generation(self));
        return;
    }

    if (orders.hasUnloadOrder)
    {
        const Cargo *cargo = registry.Get<Cargo>(self);
        if (cargo == nullptr || cargo->passengers.empty())
        {
            orders.hasUnloadOrder = false;
            OnOrderFinished(*unit, orders, mover, map, occ, self, registry.Generation(self));
            return;
        }
        if (glm::distance(cc::ToGlm(unit->position), cc::ToGlm(orders.unloadPos)) > kLoadRange)
        {
            ReissueDriverOrder(*unit, mover, map, occ, orders.unloadPos, self, registry);
            UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self,
                               registry.Generation(self));
            unit->state = UnitState::Moving;
            return;
        }
        UnloadTransport(registry, map, self, orders.unloadPos);
        orders.hasUnloadOrder = false;
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        OnOrderFinished(*unit, orders, mover, map, occ, self, registry.Generation(self));
        return;
    }

    if (orders.hasAttackGroundOrder)
    {
        const float dist =
            glm::distance(cc::ToGlm(unit->position), cc::ToGlm(orders.attackGroundPos));
        if (dist > static_cast<float>(unit->attackRange))
        {
            ReissueDriverOrder(*unit, mover, map, occ, orders.attackGroundPos, self, registry);
            UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self,
                               registry.Generation(self));
            unit->state = UnitState::Moving;
            return;
        }
        unit->state = UnitState::Attacking;
        unit->velocity = { 0.0f, 0.0f };
        if (combat.phase == AttackPhase::Ready &&
            !TurretAimed(registry, self, unit->position, orders.attackGroundPos, dtSeconds))
        {
            return; // traversing: visible aim, no free hits
        }
        TurretAimed(registry, self, unit->position, orders.attackGroundPos, dtSeconds);
        UpdateAttackPhases(*unit, combat, dtSeconds, [&] {
            ResolveStrikeGround(registry, self, *unit, combat, orders.attackGroundPos);
        });
        return;
    }

    if (mover.hasMoveOrder || mover.hasPath)
    {
        if (orders.attackMove)
        {
            combat.target = AcquireTarget(registry, self, fog, reserved);
            if (combat.target == kInvalidEntity)
            {
                combat.target = AcquireBuildingTarget(registry, self, fog);
            }
            if (combat.target != kInvalidEntity &&
                ValidateTarget(registry, *unit, combat.target, fog))
            {
                if (EngageTarget(self, *unit, combat, registry, map, combat.target, fog, dtSeconds))
                {
                    return;
                }
                if (occ != nullptr)
                {
                    IssuePathOrderFootprint(*unit, orders, mover, map, *occ,
                                            TargetPosition(registry, combat.target), self,
                                            registry.Generation(self));
                }
                else
                {
                    IssuePathOrder(*unit, orders, mover, map,
                                   TargetPosition(registry, combat.target));
                }
                UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self, registry.Generation(self));
                unit->state = UnitState::Moving;
                return;
            }
            combat.target = kInvalidEntity;
            ReissueDriverOrder(*unit, mover, map, occ, orders.attackMoveDest, self, registry);
        }
        if (combat.target != kInvalidEntity)
        {
            if (!ValidateTarget(registry, *unit, combat.target, fog))
            {
                LoseTarget(*unit, combat);
            }
            else if (EngageTarget(self, *unit, combat, registry, map, combat.target, fog,
                                  dtSeconds))
            {
                StopMoving(*unit, mover);
                return;
            }
        }
                UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self, registry.Generation(self));
        return;
    }

    if (combat.target != kInvalidEntity && !ValidateTarget(registry, *unit, combat.target, fog))
    {
        LoseTarget(*unit, combat);
    }
    if (combat.target == kInvalidEntity)
    {
        combat.target = AcquireTarget(registry, self, fog, reserved);
        if (combat.target != kInvalidEntity && orders.stance == Stance::Hold)
        {
            const Unit *sighting = registry.Get<Unit>(combat.target);
            if (sighting == nullptr || !InAttackRange(*unit, *sighting))
            {
                LoseTarget(*unit, combat);
            }
        }
    }
    if (combat.target == kInvalidEntity && orders.stance != Stance::Hold)
    {
        combat.target = AcquireBuildingTarget(registry, self, fog);
    }
    if (combat.target == kInvalidEntity)
    {
        if (orders.stance == Stance::Patrol && orders.hasPatrol && !mover.hasMoveOrder &&
            !mover.hasPath)
        {
            const Vector2 leg = orders.patrolToB ? orders.patrolB : orders.patrolA;
            orders.patrolToB = !orders.patrolToB;
            ReissueDriverOrder(*unit, mover, map, occ, leg, self, registry);
            unit->state = UnitState::Moving;
            return;
        }
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    if (EngageTarget(self, *unit, combat, registry, map, combat.target, fog, dtSeconds))
    {
        return;
    }
    if (orders.stance == Stance::Hold)
    {
        LoseTarget(*unit, combat);
        unit->state = UnitState::Idle;
        unit->velocity = { 0.0f, 0.0f };
        return;
    }

    ReissueDriverOrder(*unit, mover, map, occ, TargetPosition(registry, combat.target), self, registry);
    UpdateUnitMovement(*unit, orders, mover, combat, map, EffectiveSpeed(*unit, mover), dtSeconds, occ, self, registry.Generation(self));
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

void ResolveCrush(Registry &registry)
{
    std::vector<Entity> crushers;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.health > 0.0f && !IsEmbarked(registry, id) &&
            ActiveUnitConfig(unit.type).abilities.crushesFlesh)
        {
            crushers.push_back(id);
        }
    });
    for (const Entity crusherId : crushers)
    {
        const Unit *crusher = registry.Get<Unit>(crusherId);
        if (crusher == nullptr || crusher->health <= 0.0f)
        {
            continue;
        }
        registry.Each<Unit>([&](Entity victimId, Unit &victim) {
            if (victimId == crusherId || victim.health <= 0.0f ||
                victim.teamID == crusher->teamID || IsEmbarked(registry, victimId) ||
                !IsFleshUnit(victim.type))
            {
                return;
            }
            if (HitboxesOverlap(*crusher, victim))
            {
                victim.health = 0.0f;
            }
        });
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

void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
                          const FogOfWar *fog, float dtSeconds)
{
    ResolveCrush(registry);

    occ.ReleaseAllUnitFootprints();

    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.health > 0.0f && !IsEmbarked(registry, id))
        {
            const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));

            (void)occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
                                            registry.Generation(id));
        }
    });

    ReservedDamageMap reservedDamage;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        const CombatState *combat = FindCombatState(registry, id);
        if (unit.health > 0.0f && combat != nullptr && combat->target != kInvalidEntity &&
            (combat->phase == AttackPhase::WindUp || combat->phase == AttackPhase::Recover))
        {
            reservedDamage[combat->target] += static_cast<float>(unit.attackPower);
        }
    });

    registry.Each<Unit>([&](Entity id, Unit &) {
        UpdateUnit(id, registry, map, dtSeconds, fog, &occ, &reservedDamage);
    });

    ResolveStackedUnits(registry, map, occ);
    UpdateProjectiles(registry, map, dtSeconds);
}
