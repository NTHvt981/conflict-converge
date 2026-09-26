
#include "units/Unit.h"

#include "units/UnitInternal.h"
#include "economy/Building.h"
#include "units/Combat.h"
#include "units/Extensions.h"
#include "units/Targeting.h"
#include "world/FogOfWar.h"
#include "core/MathUtils.h"
#include "world/Pathfinder.h"
#include "app/data/UnitConfig.h"
#include "units/UnitStats.h"
#include <algorithm>
#include <cmath>
#include <vector>

void UpdateAttack(Entity self, Registry &registry, Unit &attacker, CombatState &combat,
                  Entity targetId, float dtSeconds)
{
    // M3 strike dispatch: WindUp ends in launch for arcing units, not damage.
    UpdateAttackPhases(attacker, combat, dtSeconds,
                       [&] { ResolveStrike(registry, self, attacker, combat, targetId); });
}

namespace
{

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

bool LostToFog(const Unit &unit, const Unit &target, const FogOfWar *fog)
{
    return fog != nullptr && unit.type != UnitType::Artillery &&
           !fog->IsVisible(unit.teamID, cc::WorldToTile(cc::ToGlm(target.position)));
}

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
