#include "units/Combat.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "app/UnitConfig.h"

float Effectiveness(DamageType dealt, ArmorType armor)
{
    static constexpr float kMatrix[3][3] = {
        { 1.00f, 0.75f, 0.50f },
        { 1.25f, 1.00f, 0.75f },
        { 0.50f, 1.25f, 1.00f },
    };
    return kMatrix[static_cast<int>(dealt)][static_cast<int>(armor)];
}

float ResolveAttack(Unit &attacker, CombatState &attackerCombat, Unit &defender,
                    CombatState &defenderCombat)
{
    const float effective = static_cast<float>(attacker.attackPower) *
                            Effectiveness(attacker.damageType, defender.armorType);
    defender.health -= effective;
    if (effective > 0.0f)
    {
        defenderCombat.lastDamageTaken = effective;
        defenderCombat.hitFlashTime = kHitFlashDuration;
    }
    attackerCombat.cooldown = attacker.cooldownTime;
    return effective;
}

bool IsVehicleHull(UnitType attackerType)
{
    return attackerType == UnitType::IFV || attackerType == UnitType::Artillery ||
           attackerType == UnitType::LightTank || attackerType == UnitType::HeavyTank;
}

float ResolveBuildingAttack(Unit &attacker, CombatState &attackerCombat, Building &building)
{
    const float effective = static_cast<float>(attacker.attackPower);
    building.health -= effective;
    attackerCombat.cooldown = attacker.cooldownTime;
    return effective;
}

void ResolveGroundAttack(Registry &registry, Unit &attacker, CombatState &attackerCombat,
                         Vector2 pos)
{
    Entity best = kInvalidEntity;
    float bestDistSq = 64.0f * 64.0f;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.health <= 0.0f || unit.teamID == attacker.teamID || IsEmbarked(registry, id))
        {
            return;
        }
        const float dx = (unit.position.x + 32.0f) - pos.x;
        const float dy = (unit.position.y + 32.0f) - pos.y;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq)
        {
            best = id;
            bestDistSq = distSq;
        }
    });
    if (best == kInvalidEntity)
    {
        return;
    }
    if (Unit *defender = registry.Get<Unit>(best))
    {
        ResolveAttack(attacker, attackerCombat, *defender,
                      GetCombatState(registry, best));
    }
}

Rectangle HitboxOf(const Unit &unit)
{
    return { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
}

bool HitboxesOverlap(const Unit &a, const Unit &b)
{
    return CheckCollisionRecs(HitboxOf(a), HitboxOf(b));
}

void QueryUnitsInRect(Registry &registry, Rectangle area, int teamID, std::vector<Entity> &out)
{
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.health <= 0.0f || IsEmbarked(registry, id))
        {
            return;
        }
        if (teamID >= 0 && unit.teamID != teamID)
        {
            return;
        }
        if (CheckCollisionRecs(HitboxOf(unit), area))
        {
            out.push_back(id);
        }
    });
}

namespace
{

bool IsArcing(const Unit &attacker, float &flightTime, int &splashTiles)
{
    const UnitAbilities &abilities = ActiveUnitConfig(attacker.type).abilities;
    if (abilities.arcingFlightTime > 0.0f)
    {
        flightTime = abilities.arcingFlightTime;
        splashTiles = abilities.arcingSplashTiles;
        return true;
    }
    return false;
}

void LaunchShell(Registry &registry, Unit &attacker, CombatState &attackerCombat, Vector2 landing)
{
    float flightTime = 0.0f;
    int splashTiles = 0;
    if (!IsArcing(attacker, flightTime, splashTiles))
    {
        return;
    }
    Projectile shell;
    shell.start = { attacker.position.x + 32.0f, attacker.position.y + 32.0f };
    shell.target = landing;
    shell.flightTime = flightTime;
    shell.attackPower = attacker.attackPower;
    shell.damageType = attacker.damageType;
    shell.teamID = attacker.teamID;
    shell.splashTiles = splashTiles;
    registry.Add(registry.Create(), shell);
    attackerCombat.cooldown = attacker.cooldownTime;
}

} // namespace

void ResolveStrike(Registry &registry, Entity attackerId, Unit &attacker,
                    CombatState &attackerCombat, Entity targetId)
{
    float flightTime = 0.0f;
    int splashTiles = 0;
    if (IsArcing(attacker, flightTime, splashTiles))
    {
        if (const Unit *target = registry.Get<Unit>(targetId))
        {
            LaunchShell(registry, attacker, attackerCombat,
                        { target->position.x + 32.0f, target->position.y + 32.0f });
        }
        else
        {
            attackerCombat.cooldown = attacker.cooldownTime;
        }
        (void)attackerId;
        return;
    }
    if (Unit *target = registry.Get<Unit>(targetId))
    {
        ResolveAttack(attacker, attackerCombat, *target, GetCombatState(registry, targetId));
    }
}

void ResolveStrikeGround(Registry &registry, Entity attackerId, Unit &attacker,
                         CombatState &attackerCombat, Vector2 pos)
{
    float flightTime = 0.0f;
    int splashTiles = 0;
    if (IsArcing(attacker, flightTime, splashTiles))
    {
        LaunchShell(registry, attacker, attackerCombat, pos);
        (void)attackerId;
        return;
    }
    ResolveGroundAttack(registry, attacker, attackerCombat, pos);
}

void UpdateProjectiles(Registry &registry, TileMap &map, float dtSeconds)
{
    std::vector<Entity> landed;
    registry.Each<Projectile>([&](Entity id, Projectile &shell) {
        shell.elapsed += dtSeconds;
        if (shell.elapsed >= shell.flightTime)
        {
            landed.push_back(id);
        }
    });
    for (const Entity id : landed)
    {
        const Projectile *shell = registry.Get<Projectile>(id);
        if (shell == nullptr)
        {
            continue;
        }
        const float half = static_cast<float>(shell->splashTiles) * 32.0f;
        const Rectangle area = { shell->target.x - half, shell->target.y - half, half * 2.0f,
                                 half * 2.0f };
        std::vector<Entity> victims;
        QueryUnitsInRect(registry, area, -1, victims);
        for (const Entity victimId : victims)
        {
            Unit *victim = registry.Get<Unit>(victimId);
            if (victim == nullptr || victim->teamID == shell->teamID)
            {
                continue;
            }
            const float effective = static_cast<float>(shell->attackPower) *
                                    Effectiveness(shell->damageType, victim->armorType);
            victim->health -= effective;
            if (effective > 0.0f)
            {
                GetCombatState(registry, victimId).lastDamageTaken = effective;
                GetCombatState(registry, victimId).hitFlashTime = kHitFlashDuration;
            }
        }
        std::vector<Entity> structures;
        QueryBuildingsInRect(registry, area, -1, structures);
        for (const Entity structureId : structures)
        {
            Building *building = registry.Get<Building>(structureId);
            if (building == nullptr || building->teamID == shell->teamID ||
                building->state != BuildingState::Operational)
            {
                continue;
            }
            building->health -= static_cast<float>(shell->attackPower);
            if (building->health <= 0.0f)
            {
                DemolishBuilding(registry, map, structureId);
            }
        }
        registry.Destroy(id);
    }
}
