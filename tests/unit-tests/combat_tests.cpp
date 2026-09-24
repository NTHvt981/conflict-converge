// Unit tests for damage matrix + ResolveAttack + G3 crush rule.

#include "test_harness.h"

#include "Combat.h"
#include "Extensions.h"
#include "Art.h"
#include "MathUtils.h"
#include "Registry.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitConfig.h"
#include "UnitStats.h"

#include <array>

void RunCombatTests()
{
    // --- full matrix spot values ---
    CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::STEEL) == 1.00f);
    CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::RUBBER) == 0.75f);
    CC_CHECK(Effectiveness(DamageType::KINETIC, ArmorType::COMPOSITE) == 0.50f);
    CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::STEEL) == 1.25f);
    CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::RUBBER) == 1.00f);
    CC_CHECK(Effectiveness(DamageType::EXPLOSIVE, ArmorType::COMPOSITE) == 0.75f);
    CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::STEEL) == 0.50f);
    CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::RUBBER) == 1.25f);
    CC_CHECK(Effectiveness(DamageType::ENERGY, ArmorType::COMPOSITE) == 1.00f);

    // --- scaled damage + cooldown restart ---
    Unit attacker;
    attacker.attackPower = 100;
    attacker.damageType = DamageType::KINETIC;
    attacker.cooldownTime = 1.5f;
    attacker.cooldown = 0.0f;
    Unit defender;
    defender.health = 200.0f;
    defender.armorType = ArmorType::COMPOSITE; // 0.50x vs KINETIC

    const float dealt = ResolveAttack(attacker, defender);
    CC_CHECK(dealt == 50.0f);
    CC_CHECK(defender.health == 150.0f);
    CC_CHECK(attacker.cooldown == 1.5f);

    // --- bonus matchup: EXPLOSIVE cracks STEEL ---
    Unit bomber;
    bomber.attackPower = 80;
    bomber.damageType = DamageType::EXPLOSIVE;
    bomber.cooldownTime = 2.0f;
    Unit tank;
    tank.health = 500.0f;
    tank.armorType = ArmorType::STEEL; // 1.25x vs EXPLOSIVE
    CC_CHECK(ResolveAttack(bomber, tank) == 100.0f);
    CC_CHECK(tank.health == 400.0f);

    // --- zero power deals nothing but still cycles the cooldown ---
    Unit peashooter;
    peashooter.attackPower = 0;
    peashooter.cooldownTime = 0.5f;
    Unit wall;
    wall.health = 100.0f;
    CC_CHECK(ResolveAttack(peashooter, wall) == 0.0f);
    CC_CHECK(wall.health == 100.0f);
    CC_CHECK(peashooter.cooldown == 0.5f);

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
        Unit squad;
        squad.type = UnitType::RifleInfantry;
        ApplyBaseStats(squad);
        squad.health = 100.0f;
        std::array<Vector2, 6> slots;
        float scale = 1.0f;
        CC_CHECK(SquadSlots(squad.type, 7, squad.health / 100.0f, slots, scale) == 5);
        ResolveAttack(marksman, squad); // ~25% of max HP through the matrix
        CC_CHECK(SquadSlots(squad.type, 7, squad.health / 100.0f, slots, scale) == 4);
        ResetActiveUnitConfigs();
    }
}
