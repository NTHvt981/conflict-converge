// Unit tests for anti-crush protection (vehicle vs foot contact).

#include "test_harness.h"

#include "Combat.h"

void RunCrushTests()
{
    // --- negation table: vehicles can't crush foot ---
    CC_CHECK(IsCrushNegated(UnitType::LightTank, UnitType::Infantry));
    CC_CHECK(IsCrushNegated(UnitType::HeavyTank, UnitType::AntiArmorInfantry));
    CC_CHECK(IsCrushNegated(UnitType::IFV, UnitType::Engineer));
    CC_CHECK(IsCrushNegated(UnitType::Artillery, UnitType::Infantry));

    // --- non-crush pairings still use the matrix ---
    CC_CHECK(!IsCrushNegated(UnitType::Infantry, UnitType::Infantry)); // foot vs foot
    CC_CHECK(!IsCrushNegated(UnitType::HeavyTank, UnitType::LightTank)); // ram: vehicle vs vehicle
    CC_CHECK(!IsCrushNegated(UnitType::Infantry, UnitType::HeavyTank)); // foot can't crush up
    CC_CHECK(!IsCrushNegated(UnitType::LightTank, UnitType::IFV)); // vehicle vs vehicle

    // --- negated crush: no damage, attempt still cycles the cooldown ---
    Unit tank;
    tank.type = UnitType::HeavyTank;
    tank.attackPower = 35;
    tank.damageType = DamageType::EXPLOSIVE;
    tank.cooldownTime = 1.8f;
    tank.cooldown = 0.0f;
    Unit soldier;
    soldier.type = UnitType::Infantry;
    soldier.health = 100.0f;
    soldier.armorType = ArmorType::RUBBER;
    CC_CHECK(ResolveAttack(tank, soldier, AttackContext::Crush) == 0.0f);
    CC_CHECK(soldier.health == 100.0f);
    CC_CHECK(tank.cooldown == 1.8f);

    // --- Direct fire from the same tank still hurts (guns, not treads) ---
    CC_CHECK(ResolveAttack(tank, soldier, AttackContext::Direct) > 0.0f);
    CC_CHECK(soldier.health < 100.0f);

    // --- unprotected crush (ram) deals matrix damage ---
    Unit rammer;
    rammer.type = UnitType::LightTank;
    rammer.attackPower = 20;
    rammer.damageType = DamageType::KINETIC;
    rammer.cooldownTime = 1.2f;
    Unit target;
    target.type = UnitType::IFV;
    target.health = 200.0f;
    target.armorType = ArmorType::STEEL; // 1.0x vs KINETIC
    CC_CHECK(ResolveAttack(rammer, target, AttackContext::Crush) == 20.0f);
    CC_CHECK(target.health == 180.0f);
}
