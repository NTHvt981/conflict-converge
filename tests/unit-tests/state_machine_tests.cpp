// Unit tests for M3 Goal 5: the Idle -> Moving -> Attacking driver.

#include "test_harness.h"

#include "Unit.h"

#include "Combat.h"    // expected damage via Effectiveness (M4 matrix)
#include "Targeting.h" // InAttackRange sanity in chase expectations
#include "TileMap.h"   // UpdateUnit needs the full map type (Unit.h fwd-declares)
#include "UnitStats.h" // ApplyBaseStats for real power/range/cooldowns

namespace
{

Entity AddSoldier(Registry &registry, UnitType type, int team, float x, float y)
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

void StepAll(Registry &registry, const TileMap &map, float dt, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        registry.Each<Unit>([&](Entity id, Unit &) { UpdateUnit(id, registry, map, dt); });
    }
}

} // namespace

void RunStateMachineTests()
{
    constexpr float kDt = 1.0f / 60.0f;

    // --- lone unit idles with no contacts ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = AddSoldier(registry, UnitType::Infantry, 0, 0.0f, 0.0f);
        StepAll(registry, map, kDt, 10);
        const Unit *unit = registry.Get<Unit>(id);
        CC_CHECK(unit->state == UnitState::Idle);
        CC_CHECK(unit->target == kInvalidEntity);
    }

    // --- enemy inside attack range: acquires, attacks on cooldown ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity attacker = AddSoldier(registry, UnitType::Infantry, 0, 0.0f, 0.0f);
        const Entity victim = AddSoldier(registry, UnitType::Infantry, 1, 100.0f, 0.0f);
        UpdateUnit(attacker, registry, map, kDt);
        const Unit *unit = registry.Get<Unit>(attacker);
        CC_CHECK(unit->target == victim);
        CC_CHECK(unit->state == UnitState::Attacking);
        // M4: damage is matrix-scaled, not raw power.
        const float expected = 100.0f - 10.0f * Effectiveness(DamageType::KINETIC,
                                                             registry.Get<Unit>(victim)->armorType);
        CC_CHECK(registry.Get<Unit>(victim)->health == expected);
        CC_CHECK(unit->cooldown > 0.0f);

        // Cooldown gates the next shot: one frame later, no further damage.
        const float hpAfterFirst = registry.Get<Unit>(victim)->health;
        UpdateUnit(attacker, registry, map, kDt);
        CC_CHECK(registry.Get<Unit>(victim)->health == hpAfterFirst);

        // After a full cooldown cycle, it fires again.
        StepAll(registry, map, kDt, 61);
        CC_CHECK(registry.Get<Unit>(victim)->health < hpAfterFirst);
    }

    // --- enemy in sight but out of range: chases via path order ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity chaser = AddSoldier(registry, UnitType::Infantry, 0, 0.0f, 0.0f);
        const Entity prey = AddSoldier(registry, UnitType::Infantry, 1, 256.0f, 0.0f); // > 128 range, < 320 sight
        UpdateUnit(chaser, registry, map, kDt);
        const Unit *unit = registry.Get<Unit>(chaser);
        CC_CHECK(unit->target == prey);
        CC_CHECK(unit->state == UnitState::Moving);
        CC_CHECK(unit->hasPath);

        // Walking the chase to its end arrives in attack range, attacking.
        StepAll(registry, map, kDt, 60 * 30);
        const Unit *done = registry.Get<Unit>(chaser);
        CC_CHECK(done->state == UnitState::Attacking);
        CC_CHECK(InAttackRange(*done, *registry.Get<Unit>(prey)));
    }

    // --- explicit player orders beat AI engagement ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity ordered = AddSoldier(registry, UnitType::Infantry, 0, 0.0f, 0.0f);
        AddSoldier(registry, UnitType::Infantry, 1, 100.0f, 0.0f);
        IssueMoveOrder(*registry.Get<Unit>(ordered), { 5 * 64.0f, 0.0f });
        StepAll(registry, map, kDt, 5);
        const Unit *unit = registry.Get<Unit>(ordered);
        CC_CHECK(unit->state == UnitState::Moving);
        CC_CHECK(unit->target == kInvalidEntity);
        CC_CHECK(unit->position.x > 0.0f);
    }

    // --- killing the target clears it back to Idle ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity killer = AddSoldier(registry, UnitType::HeavyTank, 0, 0.0f, 0.0f);
        const Entity doomed = AddSoldier(registry, UnitType::Engineer, 1, 100.0f, 0.0f);
        registry.Get<Unit>(doomed)->health = 1.0f;
        UpdateUnit(killer, registry, map, kDt); // fires: 35 dmg kills
        CC_CHECK(registry.Get<Unit>(doomed)->health <= 0.0f);
        UpdateUnit(killer, registry, map, kDt); // notices the corpse
        const Unit *unit = registry.Get<Unit>(killer);
        CC_CHECK(unit->target == kInvalidEntity);
        CC_CHECK(unit->state == UnitState::Idle);
    }

    // --- dead units are skipped, never crash ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity dead = AddSoldier(registry, UnitType::Infantry, 0, 0.0f, 0.0f);
        registry.Get<Unit>(dead)->health = 0.0f;
        registry.Get<Unit>(dead)->state = UnitState::Attacking;
        UpdateUnit(dead, registry, map, kDt);
        CC_CHECK(registry.Get<Unit>(dead)->state == UnitState::Attacking); // untouched
        UpdateUnit(kInvalidEntity, registry, map, kDt); // missing entity: no-op
        CC_CHECK(true);
    }
}
