// Unit tests for damage matrix + ResolveAttack + G3 crush rule.

#include "test_harness.h"

#include "units/Combat.h"
#include "units/Extensions.h"
#include "economy/Building.h"
#include "app/data/Art.h"
#include "core/MathUtils.h"
#include "core/Registry.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "app/data/UnitConfig.h"
#include "units/UnitStats.h"

#include <array>

void RunCombatTests()
{
	// --- full matrix spot values ---
	CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 1.00f);
	CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::RUBBER) == 1.00f);
	CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::COMPOSITE) == 0.60f);
	CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::STEEL) == 1.00f);
	CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::RUBBER) == 1.25f);
	CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::COMPOSITE) == 0.45f);
	CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::STEEL) == 0.50f);
	CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::RUBBER) == 1.00f);
	CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::COMPOSITE) == 1.25f);
	CC_CHECK(Effectiveness(DamageType::AP, ArmorType::STEEL) == 1.25f);
	CC_CHECK(Effectiveness(DamageType::AP, ArmorType::RUBBER) == 0.75f);
	CC_CHECK(Effectiveness(DamageType::AP, ArmorType::COMPOSITE) == 0.50f);

	// --- structure multiplier pins ---
	CC_CHECK(StructureEffectiveness(DamageType::KINETIC) == 0.50f);
	CC_CHECK(StructureEffectiveness(DamageType::EXPLOSIVE) == 1.50f);
	CC_CHECK(StructureEffectiveness(DamageType::ENERGY) == 1.00f);
	CC_CHECK(StructureEffectiveness(DamageType::AP) == 0.75f);

	// --- no-dominance regression: every row must win at least one column ---
	{
		auto Dominates = [](DamageType a, DamageType b) {
			auto Row = [](DamageType dealt) {
				return std::array<float, 3>{
					Effectiveness(dealt, ArmorType::STEEL),
					Effectiveness(dealt, ArmorType::RUBBER),
					Effectiveness(dealt, ArmorType::COMPOSITE),
				};
			};
			const std::array<float, 3> ra = Row(a);
			const std::array<float, 3> rb = Row(b);
			bool allGE = true;
			bool anyGT = false;
			for (int col = 0; col < 3; ++col)
			{
				if (ra[col] < rb[col])
					allGE = false;
				if (ra[col] > rb[col])
					anyGT = true;
			}
			return allGE && anyGT;
		};
		for (int a = 0; a < static_cast<int>(DamageType::Count); ++a)
			for (int b = 0; b < static_cast<int>(DamageType::Count); ++b)
				if (a != b)
					CC_CHECK(!Dominates(static_cast<DamageType>(a),
										static_cast<DamageType>(b)));
	}

	// --- scaled damage + cooldown restart ---
	Unit attacker;
	attacker.attackPower = 100;
	attacker.damageType = DamageType::ENERGY;
	attacker.cooldownTime = 1.5f;
	CombatState attackerCombat;
	Unit defender;
	defender.health = 200.0f;
	defender.armorType = ArmorType::STEEL; // 0.50x vs ENERGY
	CombatState defenderCombat;

	const float dealt = ResolveAttack(attacker, attackerCombat, defender, defenderCombat);
	CC_CHECK(dealt == 50.0f);
	CC_CHECK(defender.health == 150.0f);
	CC_CHECK(attackerCombat.cooldown == 1.5f);

	// --- bonus matchup: AP cracks STEEL ---
	Unit bomber;
	bomber.attackPower = 80;
	bomber.damageType = DamageType::AP;
	bomber.cooldownTime = 2.0f;
	CombatState bomberCombat;
	Unit tank;
	tank.health = 500.0f;
	tank.armorType = ArmorType::STEEL; // 1.25x vs AP
	CombatState tankCombat;
	CC_CHECK(ResolveAttack(bomber, bomberCombat, tank, tankCombat) == 100.0f);
	CC_CHECK(tank.health == 400.0f);

	// --- zero power deals nothing but still cycles the cooldown ---
	Unit peashooter;
	peashooter.attackPower = 0;
	peashooter.cooldownTime = 0.5f;
	CombatState peashooterCombat;
	Unit wall;
	wall.health = 100.0f;
	CombatState wallCombat;
	CC_CHECK(ResolveAttack(peashooter, peashooterCombat, wall, wallCombat) == 0.0f);
	CC_CHECK(wall.health == 100.0f);
	CC_CHECK(peashooterCombat.cooldown == 0.5f);

	// --- G3 crush: overlapping enemy foot dies, friendlies/vehicles spared ---
	{
		ResetActiveUnitConfigs(); // HeavyTank crushesFlesh = true by default
		Registry registry;
		Unit crusher;
		crusher.type = UnitType::HeavyTank;
		ApplyBaseStats(crusher);
		crusher.teamID = 0;
		crusher.position = cc::ToRaylib(cc::TileToWorld(3, 3));
		const Entity crusherId = registry.Create();
		registry.Add(crusherId, crusher);

		Unit victim;
		victim.type = UnitType::RifleInfantry;
		ApplyBaseStats(victim);
		victim.teamID = 1;
		victim.position = cc::ToRaylib(cc::TileToWorld(3, 3)); // same tile: overlap
		const Entity victimId = registry.Create();
		registry.Add(victimId, victim);

		Unit friendFoot;
		friendFoot.type = UnitType::RifleInfantry;
		ApplyBaseStats(friendFoot);
		friendFoot.teamID = 0; // same team: no friendly crush
		friendFoot.position = cc::ToRaylib(cc::TileToWorld(3, 3));
		const Entity friendId = registry.Create();
		registry.Add(friendId, friendFoot);

		Unit enemyVehicle;
		enemyVehicle.type = UnitType::LightTank;
		ApplyBaseStats(enemyVehicle);
		enemyVehicle.teamID = 1; // hulls are never crushed
		enemyVehicle.position = cc::ToRaylib(cc::TileToWorld(3, 3));
		const Entity vehicleId = registry.Create();
		registry.Add(vehicleId, enemyVehicle);

		ResolveCrush(registry);
		CC_CHECK(registry.Get<Unit>(victimId)->health == 0.0f);
		CC_CHECK(registry.Get<Unit>(friendId)->health > 0.0f);
		CC_CHECK(registry.Get<Unit>(vehicleId)->health > 0.0f);
		CC_CHECK(registry.Get<Unit>(crusherId)->health > 0.0f);
		ResetActiveUnitConfigs();
	}

	// --- G3 crush: non-crusher overlap never kills ---
	{
		Registry registry;
		Unit light;
		light.type = UnitType::LightTank; // no crushesFlesh
		ApplyBaseStats(light);
		light.teamID = 0;
		light.position = cc::ToRaylib(cc::TileToWorld(5, 5));
		const Entity lightId = registry.Create();
		registry.Add(lightId, light);
		Unit foot;
		foot.type = UnitType::Engineer;
		ApplyBaseStats(foot);
		foot.teamID = 1;
		foot.position = cc::ToRaylib(cc::TileToWorld(5, 5));
		const Entity footId = registry.Create();
		registry.Add(footId, foot);
		ResolveCrush(registry);
		CC_CHECK(registry.Get<Unit>(footId)->health > 0.0f);
	}

	// --- G5 sniper: one marksman hit visibly removes one squad sprite ---
	{
		ResetActiveUnitConfigs();
		CC_CHECK(ActiveUnitConfig(UnitType::AntiArmorInfantry).abilities.sniper);
		Unit marksman;
		marksman.type = UnitType::AntiArmorInfantry;
		ApplyBaseStats(marksman);
		CombatState marksmanCombat;
		Unit squad;
		squad.type = UnitType::RifleInfantry;
		ApplyBaseStats(squad);
		squad.health = 100.0f;
		squad.armorType = ArmorType::STEEL; // AP deals 31.25 (31% of max HP)
		CombatState squadCombat;
		std::array<Vector2, 6> slots;
		float scale = 1.0f;
		CC_CHECK(SquadSlots(squad.type, 7, squad.health / 100.0f, slots, scale) == 5);
		ResolveAttack(marksman, marksmanCombat, squad, squadCombat); // ~25% of max HP through the matrix
		CC_CHECK(SquadSlots(squad.type, 7, squad.health / 100.0f, slots, scale) == 4);
		ResetActiveUnitConfigs();
	}

	// --- building hits scale by damage type through StructureEffectiveness ---
	{
		const DamageType damageTypes[] = { DamageType::KINETIC, DamageType::EXPLOSIVE,
										   DamageType::ENERGY, DamageType::AP };
		const float expectedDamage[] = { 5.0f, 15.0f, 10.0f, 7.5f };
		for (int i = 0; i < 4; ++i)
		{
			Unit attacker;
			attacker.attackPower = 10;
			attacker.damageType = damageTypes[i];
			attacker.cooldownTime = 2.5f;
			CombatState attackerCombat;
			Building building;
			building.health = 400.0f;
			const float dealt =
				ResolveBuildingAttack(attacker, attackerCombat, building);
			CC_CHECK(dealt == expectedDamage[i]);
			CC_CHECK(building.health == 400.0f - expectedDamage[i]);
			CC_CHECK(attackerCombat.cooldown == attacker.cooldownTime);
			CC_CHECK(building.hitFlashTime == kHitFlashDuration);
		}
	}
}
