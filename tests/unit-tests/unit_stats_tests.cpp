// Unit tests for the 9-type base stat table.

#include "test_harness.h"

#include "Extensions.h"
#include "UnitStats.h"

void RunUnitStatsTests()
{
    // --- every type has a sane (positive) stat line ---
    // Support types (Medic, Engineer) cannot attack: attackPower 0 and no
    // attack range; everything else must reach past its own tile.
    const UnitType all[] = {
        UnitType::RifleInfantry,
        UnitType::AntiArmorInfantry,
        UnitType::Engineer,
        UnitType::IFV,
        UnitType::Artillery,
        UnitType::LightTank,
        UnitType::HeavyTank,
        UnitType::Medic,
    };
    for (UnitType type : all)
    {
        const UnitStats &stats = BaseStats(type);
        CC_CHECK(stats.health > 0.0f);
        CC_CHECK(stats.cooldownTime > 0.0f);
        CC_CHECK(stats.speed > 0.0f);
        CC_CHECK(stats.sightRange > 0.0f);
        if (stats.attackPower > 0)
        {
            CC_CHECK(stats.attackRange > 0);
        }
    }

    // --- spot values pin the table (change deliberately, not by accident) ---
    CC_CHECK(BaseStats(UnitType::RifleInfantry).health == 100.0f);
    CC_CHECK(BaseStats(UnitType::HeavyTank).health == 500.0f);
    CC_CHECK(BaseStats(UnitType::Artillery).attackRange == 384);
    CC_CHECK(BaseStats(UnitType::IFV).speed == 128.0f);
    CC_CHECK(BaseStats(UnitType::AntiArmorInfantry).damageType == DamageType::EXPLOSIVE);
    CC_CHECK(BaseStats(UnitType::HeavyTank).armorType == ArmorType::COMPOSITE);
    // Support types cannot attack (heal/repair instead).
    CC_CHECK(BaseStats(UnitType::Medic).attackPower == 0);
    CC_CHECK(BaseStats(UnitType::Medic).health == 60.0f);
    CC_CHECK(BaseStats(UnitType::Engineer).attackPower == 0);

    // --- ordering sanity: heavies outlive lights, artillery outranges all ---
    CC_CHECK(BaseStats(UnitType::HeavyTank).health > BaseStats(UnitType::LightTank).health);
    CC_CHECK(BaseStats(UnitType::LightTank).health > BaseStats(UnitType::RifleInfantry).health);
    for (UnitType type : all)
    {
        if (type != UnitType::Artillery)
        {
            CC_CHECK(BaseStats(UnitType::Artillery).attackRange > BaseStats(type).attackRange);
        }
    }

    // --- ApplyBaseStats fills stats but preserves identity/placement ---
    Unit unit;
    unit.type = UnitType::LightTank;
    unit.teamID = 2;
    unit.position = { 128.0f, 192.0f };
    unit.isSelected = true;
    ApplyBaseStats(unit);
    CC_CHECK(unit.health == 300.0f);
    CC_CHECK(unit.attackPower == 20);
    CC_CHECK(unit.speed == 96.0f);
    CC_CHECK(CombatState{}.cooldown == 0.0f); // live cooldown starts at zero, not from stats
    CC_CHECK(unit.teamID == 2);
    CC_CHECK(unit.position.x == 128.0f);
    CC_CHECK(unit.position.y == 192.0f);
    CC_CHECK(unit.isSelected);
}
