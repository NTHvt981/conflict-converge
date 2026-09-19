#include "Combat.h"

#include "Building.h"

float Effectiveness(DamageType dealt, ArmorType armor)
{
    static constexpr float kMatrix[3][3] = {
        { 1.00f, 0.75f, 0.50f },
        { 1.25f, 1.00f, 0.75f },
        { 0.50f, 1.25f, 1.00f },
    };
    return kMatrix[static_cast<int>(dealt)][static_cast<int>(armor)];
}

float ResolveAttack(Unit &attacker, Unit &defender)
{
    const float effective = static_cast<float>(attacker.attackPower) *
                            Effectiveness(attacker.damageType, defender.armorType);
    defender.health -= effective;
    if (effective > 0.0f)
    {
        defender.lastDamageTaken = effective;
        defender.hitFlashTime = kHitFlashDuration;
    }
    attacker.cooldown = attacker.cooldownTime;
    return effective;
}

bool IsVehicleHull(UnitType attackerType)
{
    return attackerType == UnitType::IFV || attackerType == UnitType::Artillery ||
           attackerType == UnitType::LightTank || attackerType == UnitType::HeavyTank;
}

float ResolveBuildingAttack(Unit &attacker, Building &building)
{
    const float effective = static_cast<float>(attacker.attackPower);
    building.health -= effective;
    attacker.cooldown = attacker.cooldownTime;
    return effective;
}

void ResolveGroundAttack(Registry &registry, Unit &attacker, Vector2 pos)
{
    Entity best = kInvalidEntity;
    float bestDistSq = 64.0f * 64.0f;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.health <= 0.0f || unit.teamID == attacker.teamID)
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
        ResolveAttack(attacker, *defender);
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
        if (unit.health <= 0.0f)
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
