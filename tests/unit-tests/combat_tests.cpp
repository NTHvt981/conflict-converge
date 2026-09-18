// Unit tests for damage matrix + ResolveAttack.

#include "test_harness.h"

#include "Combat.h"

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
}
