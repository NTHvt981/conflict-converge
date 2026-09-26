#pragma once

#include "units/Unit.h"

// Ranges and sight are in pixels (64px tiles).

struct UnitStats
{
    float health = 100.0f;
    ArmorType armorType = ArmorType::STEEL;
    DamageType damageType = DamageType::KINETIC;
    int attackPower = 0;
    int attackRange = 0;
    float cooldownTime = 1.0f;
    float speed = 64.0f;
    float sightRange = 320.0f;
};

const UnitStats &BaseStats(UnitType type);

// Preserves position, team, selection, and orders.
void ApplyBaseStats(Unit &unit);
