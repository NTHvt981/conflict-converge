// Unit tests for the 9-type base stat table.

#include "test_harness.h"

#include "units/Extensions.h"
#include "units/UnitStats.h"
#include "app/data/UnitConfig.h"
#include "app/data/UnitConfigDefaults.h"
#include "units/UnitTypeInfo.h"

void RunUnitStatsTests()
{
	ResetActiveUnitConfigs();
	for (int i = 0; i < static_cast<int>(UnitType::Count); ++i)
	{
		const UnitType type = static_cast<UnitType>(i);
		CC_CHECK(ActiveUnitConfig(type).type == InfoOf(type).configName);
	}

	// Support types (Medic, Engineer) cannot attack; the rest out-range their tile.
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
			CC_CHECK(stats.attackRange > 0.0f);
		}
	}

	CC_CHECK(BaseStats(UnitType::RifleInfantry).health == 100.0f);
	CC_CHECK(BaseStats(UnitType::HeavyTank).health == 500.0f);
	CC_CHECK(BaseStats(UnitType::Artillery).attackRange == 6.0f);
	CC_CHECK(BaseStats(UnitType::IFV).speed == 2.0f);
	CC_CHECK(BaseStats(UnitType::AntiArmorInfantry).damageType == DamageType::EXPLOSIVE);
	CC_CHECK(BaseStats(UnitType::HeavyTank).armorType == ArmorType::COMPOSITE);
	CC_CHECK(BaseStats(UnitType::Medic).attackPower == 0);
	CC_CHECK(BaseStats(UnitType::Medic).health == 60.0f);
	CC_CHECK(BaseStats(UnitType::Engineer).attackPower == 0);

	CC_CHECK(BaseStats(UnitType::HeavyTank).health > BaseStats(UnitType::LightTank).health);
	CC_CHECK(BaseStats(UnitType::LightTank).health > BaseStats(UnitType::RifleInfantry).health);
	for (UnitType type : all)
	{
		if (type != UnitType::Artillery)
		{
			CC_CHECK(BaseStats(UnitType::Artillery).attackRange >
					 BaseStats(type).attackRange);
		}
	}

	Unit unit;
	unit.type = UnitType::LightTank;
	unit.teamID = kEnemyTeam;
	unit.position = { 128.0f, 192.0f };
	unit.isSelected = true;
	ApplyBaseStats(unit);
	CC_CHECK(unit.health == 300.0f);
	CC_CHECK(unit.attackPower == 20);
	CC_CHECK(unit.speed == 192.0f); // 1.5 tiles/s against TILE_SIZE 128
	CC_CHECK(unit.attackRange == 384); // 3.0 tiles
	CC_CHECK(unit.sightRange == 640.0f); // 5.0 tiles
	CC_CHECK(CombatState{}.cooldown == 0.0f); // live cooldown starts at zero, not from stats
	CC_CHECK(unit.teamID == kEnemyTeam);
	CC_CHECK(unit.position.x == 128.0f);
	CC_CHECK(unit.position.y == 192.0f);
	CC_CHECK(unit.isSelected);

	{
		UnitConfig rifle = DefaultUnitConfig(UnitType::RifleInfantry);
		rifle.stats.health = 77.0f;
		rifle.stats.attackRange = 2.5f;
		rifle.stats.speed = 1.5f;
		SetActiveUnitConfigs(std::vector<UnitConfig>{ rifle });
		Unit tuned;
		tuned.type = UnitType::RifleInfantry;
		ApplyBaseStats(tuned);
		CC_CHECK(tuned.health == 77.0f);
		CC_CHECK(tuned.attackRange == 320); // 2.5 tiles, lround against TILE_SIZE 128
		CC_CHECK(tuned.speed == 192.0f);    // 1.5 tiles/s
		CC_CHECK(tuned.footprintWidth == UnitFootprint(UnitType::RifleInfantry).x);
		ResetActiveUnitConfigs();
	}
}
