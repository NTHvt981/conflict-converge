#include "Combat.h"

#include "Building.h" // M13: structural damage targets

float Effectiveness(DamageType dealt, ArmorType armor)
{
    // Rows: attacker damage type. Columns: defender armor.
    // KINETIC is baseline; EXPLOSIVE cracks STEEL but COMPOSITE shrugs it;
    // ENERGY melts RUBBER but bounces off STEEL.
    static constexpr float kMatrix[3][3] = {
        // STEEL  RUBBER  COMPOSITE
        { 1.00f, 0.75f, 0.50f }, // KINETIC
        { 1.25f, 1.00f, 0.75f }, // EXPLOSIVE
        { 0.50f, 1.25f, 1.00f }, // ENERGY
    };
    return kMatrix[static_cast<int>(dealt)][static_cast<int>(armor)];
}

float ResolveAttack(Unit &attacker, Unit &defender, AttackContext context)
{
    if (context == AttackContext::Crush && IsCrushNegated(attacker.type, defender.type))
    {
        attacker.cooldown = attacker.cooldownTime; // the attempt still cycles
        return 0.0f;
    }
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

bool IsCrushNegated(UnitType attackerType, UnitType defenderType)
{
    const bool vehicleAttacker = IsVehicleHull(attackerType);
    const bool footDefender = defenderType == UnitType::Infantry ||
                              defenderType == UnitType::AntiArmorInfantry || defenderType == UnitType::Engineer;
    return vehicleAttacker && footDefender;
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
    // Nearest live enemy to the impact point inside one tile. Attacker's
    // own team and corpses never qualify (matches QueryUnitsInRect's
    // exclusion semantics, plus a nearest-wins pick on top).
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
        return; // clean miss: the windup/cooldown still cycled
    }
    if (Unit *defender = registry.Get<Unit>(best))
    {
        ResolveAttack(attacker, *defender, AttackContext::Direct);
    }
}

// M4 Goal 2: the body fills the center 32x32 of the unit's 64x64 tile.
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
            return; // corpses awaiting factory teardown don't collide
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
