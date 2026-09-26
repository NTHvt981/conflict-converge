// Unit tests for damage feedback (hit stamp + flash decay).

#include "test_harness.h"

#include "units/Combat.h"
#include "units/Extensions.h"
#include "world/TileMap.h"

namespace
{

Unit MakeGun(float power = 10.0f)
{
    Unit gun;
    gun.type = UnitType::LightTank;
    gun.damageType = DamageType::KINETIC;
    gun.attackPower = static_cast<int>(power);
    gun.cooldownTime = 1.0f;
    return gun;
}

Unit MakeVictim()
{
    Unit victim;
    victim.type = UnitType::LightTank; // STEEL armor: KINETIC x1.0
    victim.armorType = ArmorType::STEEL;
    victim.health = 100.0f;
    victim.position = cc::ToRaylib(cc::TileToWorld(2, 2));
    return victim;
}

} // namespace

void RunFeedbackTests()
{
    // --- a landed hit stamps the victim's feedback fields ---
    {
        Unit gun = MakeGun();
        CombatState gunCombat;
        Unit victim = MakeVictim();
        CombatState victimCombat;
        const float dealt = ResolveAttack(gun, gunCombat, victim, victimCombat);
        CC_CHECK(dealt == 10.0f);
        CC_CHECK(victimCombat.lastDamageTaken == dealt);
        CC_CHECK(victimCombat.hitFlashTime == kHitFlashDuration);
    }

    // --- matrix-scaled number is what gets stamped ---
    {
        Unit gun = MakeGun();
        gun.damageType = DamageType::ENERGY; // ENERGY vs STEEL: 0.5x
        CombatState gunCombat;
        Unit victim = MakeVictim();
        CombatState victimCombat;
        const float dealt = ResolveAttack(gun, gunCombat, victim, victimCombat);
        CC_CHECK(dealt == 5.0f);
        CC_CHECK(victimCombat.lastDamageTaken == 5.0f);
    }

    // --- zero-power hit cycles cooldown but shows no feedback ---
    {
        Unit gun = MakeGun(0.0f);
        CombatState gunCombat;
        Unit victim = MakeVictim();
        CombatState victimCombat;
        CC_CHECK(ResolveAttack(gun, gunCombat, victim, victimCombat) == 0.0f);
        CC_CHECK(victimCombat.lastDamageTaken == 0.0f);
        CC_CHECK(victimCombat.hitFlashTime == 0.0f);
    }

    // --- flash decays through the AI driver and clamps at zero ---
    {
        Registry registry;
        TileMap map(20, 15);
        Unit victim = MakeVictim();
        const Entity id = registry.Create();
        registry.Add(id, victim);
        CombatState flash;
        flash.hitFlashTime = kHitFlashDuration;
        flash.lastDamageTaken = 10.0f;
        registry.Add(id, flash);

        constexpr float kDt = 1.0f / 60.0f;
        UpdateUnit(id, registry, map, kDt);
        const float afterOne = FindCombatState(registry, id)->hitFlashTime;
        CC_CHECK(afterOne < kHitFlashDuration && afterOne > 0.0f);

        for (int i = 0; i < 20; ++i)
        {
            UpdateUnit(id, registry, map, kDt);
        }
        CC_CHECK(FindCombatState(registry, id)->hitFlashTime == 0.0f);
        // The number outlives the flash (render stops showing it with the overlay).
        CC_CHECK(FindCombatState(registry, id)->lastDamageTaken == 10.0f);
    }
}
