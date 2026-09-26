#include "units/UnitStats.h"

namespace
{

const UnitStats kTable[] = {
    // RifleInfantry, AntiArmorInfantry, Engineer (support: no attack),
    // IFV, Artillery, LightTank, HeavyTank, PrototypeInfantry, Medic
    // (support: no attack, heals flesh instead).
    { 100.0f, ArmorType::RUBBER, DamageType::KINETIC, 10, 128, 1.0f, 64.0f, 320.0f },
    { 90.0f, ArmorType::RUBBER, DamageType::EXPLOSIVE, 25, 128, 1.5f, 64.0f, 320.0f },
    { 60.0f, ArmorType::RUBBER, DamageType::KINETIC, 0, 64, 1.0f, 64.0f, 256.0f },
    { 200.0f, ArmorType::STEEL, DamageType::KINETIC, 15, 192, 0.8f, 128.0f, 384.0f },
    { 150.0f, ArmorType::STEEL, DamageType::EXPLOSIVE, 40, 384, 3.0f, 48.0f, 320.0f },
    { 300.0f, ArmorType::STEEL, DamageType::KINETIC, 20, 192, 1.2f, 96.0f, 320.0f },
    { 500.0f, ArmorType::COMPOSITE, DamageType::EXPLOSIVE, 35, 224, 1.8f, 64.0f, 320.0f },
    { 100.0f, ArmorType::RUBBER, DamageType::KINETIC, 10, 128, 1.0f, 64.0f, 320.0f },
    { 60.0f, ArmorType::RUBBER, DamageType::KINETIC, 0, 0, 1.0f, 64.0f, 256.0f },
};

static_assert(sizeof(kTable) / sizeof(kTable[0]) == 9, "one stat line per UnitType");

}

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
    unit.speed = stats.speed;
    unit.sightRange = stats.sightRange;
    const bool isVehicle = unit.type == UnitType::IFV || unit.type == UnitType::Artillery ||
                           unit.type == UnitType::LightTank || unit.type == UnitType::HeavyTank;
    unit.footprintWidth = isVehicle ? 2 : 1;
    unit.footprintHeight = isVehicle ? 2 : 1;
}
