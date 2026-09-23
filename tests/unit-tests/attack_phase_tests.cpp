// Unit tests for attack phases (Ready/WindUp/Recover + move cancel).

#include "test_harness.h"

#include "Combat.h"   // expected damage via Effectiveness
#include "Targeting.h" // InAttackRange sanity
#include "TileMap.h"  // UpdateUnit needs the full map type
#include "Unit.h"
#include "UnitStats.h" // ApplyBaseStats for real power/range/cooldowns

namespace
{

Entity AddPhaser(Registry &registry, UnitType type, int team, float x, float y)
{
    Unit unit;
    unit.type = type;
    ApplyBaseStats(unit);
    unit.teamID = team;
    unit.position = { x, y };
    const Entity id = registry.Create();
    registry.Add(id, unit);
    return id;
}

void StepPhasers(Registry &registry, TileMap &map, float dt, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        registry.Each<Unit>([&](Entity id, Unit &) { UpdateUnit(id, registry, map, dt); });
    }
}

} // namespace

void RunAttackPhaseTests()
{
    constexpr float kDt = 1.0f / 60.0f;

    // --- full strike cycle: telegraph, single hit, recover, re-fire ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity attacker = AddPhaser(registry, UnitType::RifleInfantry, 0, 0.0f, 0.0f);
        const Entity victim = AddPhaser(registry, UnitType::RifleInfantry, 1, 100.0f, 0.0f);

        UpdateUnit(attacker, registry, map, kDt);
        CC_CHECK(registry.Get<Unit>(attacker)->phase == AttackPhase::WindUp);
        CC_CHECK(registry.Get<Unit>(victim)->health == 100.0f); // telegraphed, not landed

        StepPhasers(registry, map, kDt, 4); // mid-windup: still nothing
        CC_CHECK(registry.Get<Unit>(victim)->health == 100.0f);

        StepPhasers(registry, map, kDt, 6); // 0.15s windup expires: hit lands once
        const float expected = 100.0f - 10.0f * Effectiveness(DamageType::KINETIC,
                                                             registry.Get<Unit>(victim)->armorType);
        CC_CHECK(registry.Get<Unit>(victim)->health == expected);
        CC_CHECK(registry.Get<Unit>(attacker)->phase == AttackPhase::Recover);
        CC_CHECK(registry.Get<Unit>(attacker)->cooldown > 0.0f);

        const float hpAfterFirst = registry.Get<Unit>(victim)->health;
        StepPhasers(registry, map, kDt, 30); // recovering: no second hit
        CC_CHECK(registry.Get<Unit>(victim)->health == hpAfterFirst);

        StepPhasers(registry, map, kDt, 61); // cooled: Ready -> WindUp -> second hit
        CC_CHECK(registry.Get<Unit>(victim)->health < hpAfterFirst);
    }

    // --- unarmed units never leave Ready ---
    {
        Registry registry;
        TileMap map(10, 10);
        Unit unarmed;
        unarmed.attackPower = 0;
        unarmed.attackRange = 256;
        unarmed.sightRange = 320.0f;
        unarmed.teamID = 0;
        unarmed.position = { 0.0f, 0.0f };
        const Entity pacifist = registry.Create();
        registry.Add(pacifist, unarmed);
        AddPhaser(registry, UnitType::RifleInfantry, 1, 100.0f, 0.0f);
        StepPhasers(registry, map, kDt, 30);
        const Unit *unit = registry.Get<Unit>(pacifist);
        CC_CHECK(unit->phase == AttackPhase::Ready);
        CC_CHECK(unit->state == UnitState::Attacking); // engaged, but holds fire
    }

    // --- moving cancels the telegraph ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity walker = AddPhaser(registry, UnitType::RifleInfantry, 0, 0.0f, 0.0f);
        const Entity gone = AddPhaser(registry, UnitType::RifleInfantry, 1, 100.0f, 0.0f);
        UpdateUnit(walker, registry, map, kDt); // acquires, starts WindUp
        CC_CHECK(registry.Get<Unit>(walker)->phase == AttackPhase::WindUp);
        registry.Destroy(gone); // target lost...
        IssueMoveOrder(*registry.Get<Unit>(walker), { 5 * 64.0f, 0.0f }); // ...walk instead
        StepPhasers(registry, map, kDt, 3);
        const Unit *unit = registry.Get<Unit>(walker);
        CC_CHECK(unit->phase == AttackPhase::Ready);
        CC_CHECK(unit->state == UnitState::Moving);
        CC_CHECK(unit->position.x > 0.0f);
    }
}
