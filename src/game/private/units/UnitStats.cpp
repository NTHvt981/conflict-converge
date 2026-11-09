#include "units/UnitStats.h"

#include "core/MathUtils.h"
#include "units/UnitTypeInfo.h"

#include <cmath>

namespace
{

const UnitStats kTable[] = {
	// RifleInfantry, AntiArmorInfantry, Engineer (support: no attack),
	// IFV, Artillery, LightTank, HeavyTank, PrototypeInfantry, Medic
	// (support: no attack, heals flesh instead).
	{ 100.0f, ArmorType::RUBBER, DamageType::KINETIC, 10, 2.0f, 1.0f, 1.0f, 5.0f },
	{ 90.0f, ArmorType::RUBBER, DamageType::EXPLOSIVE, 25, 2.0f, 1.5f, 1.0f, 5.0f },
	{ 60.0f, ArmorType::RUBBER, DamageType::KINETIC, 0, 1.0f, 1.0f, 1.0f, 4.0f },
	{ 200.0f, ArmorType::STEEL, DamageType::KINETIC, 15, 3.0f, 0.8f, 2.0f, 6.0f },
	{ 150.0f, ArmorType::STEEL, DamageType::EXPLOSIVE, 40, 6.0f, 3.0f, 0.75f, 5.0f },
	{ 300.0f, ArmorType::STEEL, DamageType::KINETIC, 20, 3.0f, 1.2f, 1.5f, 5.0f },
	{ 500.0f, ArmorType::COMPOSITE, DamageType::EXPLOSIVE, 35, 3.5f, 1.8f, 1.0f, 5.0f },
	{ 100.0f, ArmorType::RUBBER, DamageType::KINETIC, 10, 2.0f, 1.0f, 1.0f, 5.0f },
	{ 60.0f, ArmorType::RUBBER, DamageType::KINETIC, 0, 0.0f, 1.0f, 1.0f, 4.0f },
};

static_assert(sizeof(kTable) / sizeof(kTable[0]) == 9, "one stat line per UnitType");

}

const UnitStats &BaseStats(UnitType type)
{
	return kTable[static_cast<int>(type)];
}

cc::IVec2 UnitFootprint(UnitType type)
{
	return InfoOf(type).footprint;
}

void ApplyBaseStats(Unit &unit)
{
	const UnitStats &stats = BaseStats(unit.type);
	unit.health = stats.health;
	unit.armorType = stats.armorType;
	unit.damageType = stats.damageType;
	unit.attackPower = stats.attackPower;
	unit.attackRange = static_cast<int>(std::lroundf(stats.attackRange * cc::TILE_SIZE));
	unit.cooldownTime = stats.cooldownTime;
	unit.speed = stats.speed * cc::TILE_SIZE;
	unit.sightRange = stats.sightRange * cc::TILE_SIZE;
	const cc::IVec2 fp = UnitFootprint(unit.type);
	unit.footprintWidth = fp.x;
	unit.footprintHeight = fp.y;
}

ProductionCategory ProductionCategoryOf(UnitType type)
{
	return InfoOf(type).isVehicle ? ProductionCategory::Vehicle : ProductionCategory::Infantry;
}
