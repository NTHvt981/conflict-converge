#include "UnitStats.h"

namespace
{

// Order matches UnitType enum: Infantry, AntiArmorInfantry, Engineer, IFV,
// Artillery, LightTank, HeavyTank. Placeholder balance (M3G2); M4 retunes.
const UnitStats kTable[] = {
    { 100.0f, ArmorType::RUBBER, DamageType::KINETIC, 10, 128, 1.0f, 64.0f, 320.0f }, // Infantry
    { 90.0f, ArmorType::RUBBER, DamageType::EXPLOSIVE, 25, 128, 1.5f, 64.0f, 320.0f }, // AntiArmorInfantry
    { 60.0f, ArmorType::RUBBER, DamageType::KINETIC, 2, 64, 1.0f, 64.0f, 256.0f }, // Engineer
    { 200.0f, ArmorType::STEEL, DamageType::KINETIC, 15, 192, 0.8f, 128.0f, 384.0f }, // IFV
    { 150.0f, ArmorType::STEEL, DamageType::EXPLOSIVE, 40, 384, 3.0f, 48.0f, 320.0f }, // Artillery
    { 300.0f, ArmorType::STEEL, DamageType::KINETIC, 20, 192, 1.2f, 96.0f, 320.0f }, // LightTank
    { 500.0f, ArmorType::COMPOSITE, DamageType::EXPLOSIVE, 35, 224, 1.8f, 64.0f, 320.0f }, // HeavyTank
};

static_assert(sizeof(kTable) / sizeof(kTable[0]) == 7, "one stat line per UnitType");

} // namespace

const UnitStats &BaseStats(UnitType type)
{
    return kTable[static_cast<int>(type)];
}

void ApplyBaseStats(Unit &unit)
{
    const UnitStats &stats = BaseStats(unit.type);
    unit.health = stats.health;
    unit.armorType = stats.armorType;
    unit.damageType = stats.damageType;
    unit.attackPower = stats.attackPower;
    unit.attackRange = stats.attackRange;
    unit.cooldownTime = stats.cooldownTime;
    unit.cooldown = 0.0f;
    unit.speed = stats.speed;
    unit.sightRange = stats.sightRange;
}
