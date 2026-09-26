#include "units/Unit.h"

#include "units/UnitInternal.h"
#include "units/Combat.h"
#include "units/Extensions.h"
#include "world/FogOfWar.h"
#include "core/MathUtils.h"
#include "world/Pathfinder.h"
#include "units/Targeting.h"
#include "world/TileMap.h"
#include "economy/Building.h"
#include "units/UnitStats.h"

#include <unordered_map>

namespace
{

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

constexpr float kLoadRange = 128.0f;

constexpr float kRepairRange = 128.0f;
constexpr float kRepairRate = 15.0f;

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
