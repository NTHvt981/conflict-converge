#pragma once

#include "Unit.h" // UnitType, Unit, ArmorType, DamageType

// M3 Goal 2: base stats for the 7 unit types (milestone: "Define base stats
// for each type"). Numbers are conservative C&C/CoH-inspired placeholders —
// see MILESTONES.md blockings; M4 tuning will adjust them against the damage
// matrix. Ranges and sight are in pixels (64px tiles).

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

// Stat line for a type (returned by reference to a static table).
const UnitStats &BaseStats(UnitType type);

// Copy the type's stats onto a unit; position, team, selection and orders
// are preserved (only stat fields are overwritten).
void ApplyBaseStats(Unit &unit);
