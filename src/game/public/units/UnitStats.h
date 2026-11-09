#pragma once

#include "units/Unit.h"

// Distances are in tiles, speed in tiles per second; ApplyBaseStats
// converts both to world pixels against the current TILE_SIZE.

struct UnitStats
{
	float health = 100.0f;
	ArmorType armorType = ArmorType::STEEL;
	DamageType damageType = DamageType::KINETIC;
	int attackPower = 0;
	float attackRangeTiles = 0.0f;
	float cooldownTime = 1.0f;
	float speedTilesPerSec = 1.0f;
	float sightRangeTiles = 5.0f;
};

const UnitStats &BaseStats(UnitType type);

// Anchor footprint in tiles: LightTank is 1x1; IFV/Artillery/HeavyTank span 2x2.
cc::IVec2 UnitFootprint(UnitType type);

// Preserves position, team, selection, and orders.
void ApplyBaseStats(Unit &unit);

enum class ProductionCategory
{
	Infantry,
	Vehicle
};
ProductionCategory ProductionCategoryOf(UnitType type);
