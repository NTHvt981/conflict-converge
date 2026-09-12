#include "Combat.h"

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

float ResolveAttack(Unit &attacker, Unit &defender)
{
    const float effective = static_cast<float>(attacker.attackPower) *
                            Effectiveness(attacker.damageType, defender.armorType);
    defender.health -= effective;
    attacker.cooldown = attacker.cooldownTime;
    return effective;
}
