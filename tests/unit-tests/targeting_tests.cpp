// Unit tests for threat-priority acquisition and range checks.

#include "test_harness.h"

#include "units/Targeting.h"

#include "units/UnitStats.h" // ApplyBaseStats for realistic power/sight values

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

} // namespace

void RunTargetingTests()
{
    // --- distance helper: 3-4-5 triangle ---
    Unit a;
    a.position = { 0.0f, 0.0f };
    Unit b;
    b.position = { 3.0f, 4.0f };
    CC_CHECK(CcNear(DistanceBetween(a, b), 5.0f));

    // --- range check with RifleInfantry (range 128) ---
    Unit attacker;
    attacker.type = UnitType::RifleInfantry;
    ApplyBaseStats(attacker);
    Unit close;
    close.position = { 100.0f, 0.0f };
    Unit far;
    far.position = { 200.0f, 0.0f };
    Unit edge;
    edge.position = { 128.0f, 0.0f };
    CC_CHECK(InAttackRange(attacker, close));
    CC_CHECK(!InAttackRange(attacker, far));
    CC_CHECK(InAttackRange(attacker, edge)); // boundary counts as in range

    // --- acquisition: threat (power) beats proximity ---
    Registry registry;
    const Entity seeker = AddSoldier(registry, UnitType::RifleInfantry, 0, 0.0f, 0.0f);
    const Entity weakNear = AddSoldier(registry, UnitType::RifleInfantry, 1, 100.0f, 0.0f);
    registry.Get<Unit>(weakNear)->attackPower = 2; // weak
    const Entity strongFar = AddSoldier(registry, UnitType::LightTank, 1, 200.0f, 0.0f); // pow 20
    CC_CHECK(AcquireTarget(registry, seeker) == strongFar);

    // --- equal threat: nearest wins ---
    registry.Get<Unit>(strongFar)->attackPower = 2;
    CC_CHECK(AcquireTarget(registry, seeker) == weakNear);

    // --- target priority: armor hunters prefer vehicles at equal power ---
    {
        Registry duel;
        const Entity heavy = AddSoldier(duel, UnitType::HeavyTank, 0, 0.0f, 0.0f);
        const Entity tank = AddSoldier(duel, UnitType::LightTank, 1, 200.0f, 0.0f);
        const Entity foot = AddSoldier(duel, UnitType::RifleInfantry, 1, 100.0f, 0.0f);
        duel.Get<Unit>(tank)->attackPower = 10;
        duel.Get<Unit>(foot)->attackPower = 10;
        CC_CHECK(AcquireTarget(duel, heavy) == tank); // 10*1.5 beats 10*1.0
        const Entity grunt = AddSoldier(duel, UnitType::RifleInfantry, 0, 0.0f, 0.0f);
        CC_CHECK(AcquireTarget(duel, grunt) == foot); // neutral: nearest wins
        CC_CHECK(TargetPriorityWeight(UnitType::AntiArmorInfantry, UnitType::HeavyTank) > 1.0f);
        CC_CHECK(TargetPriorityWeight(UnitType::RifleInfantry, UnitType::HeavyTank) == 1.0f);
        CC_CHECK(TargetPriorityWeight(UnitType::HeavyTank, UnitType::RifleInfantry) == 1.0f);
    }

    // --- allies, the dead, and the out-of-sight are ignored ---
    AddSoldier(registry, UnitType::HeavyTank, 0, 50.0f, 0.0f); // ally, pow 35
    const Entity corpse = AddSoldier(registry, UnitType::HeavyTank, 1, 60.0f, 0.0f);
    registry.Get<Unit>(corpse)->health = 0.0f;
    AddSoldier(registry, UnitType::HeavyTank, 1, 1000.0f, 0.0f); // beyond sight 320
    CC_CHECK(AcquireTarget(registry, seeker) == weakNear);

    // --- nobody qualifies: invalid ---
    registry.Destroy(weakNear);
    registry.Destroy(strongFar);
    CC_CHECK(AcquireTarget(registry, seeker) == kInvalidEntity);

    // --- missing seeker / blind seeker: invalid ---
    CC_CHECK(AcquireTarget(registry, kInvalidEntity) == kInvalidEntity);
    CC_CHECK(AcquireTarget(registry, 9999) == kInvalidEntity);
    Unit blind;
    blind.attackPower = 10;
    blind.sightRange = 0.0f;
    const Entity blindId = registry.Create();
    registry.Add(blindId, blind);
    CC_CHECK(AcquireTarget(registry, blindId) == kInvalidEntity);

    // --- overkill protection: reserved-lethal targets are skipped ---
    {
        Registry battle;
        const Entity hunter = AddSoldier(battle, UnitType::RifleInfantry, 0, 0.0f, 0.0f);
        const Entity doomed = AddSoldier(battle, UnitType::LightTank, 1, 100.0f, 0.0f);
        const Entity healthy = AddSoldier(battle, UnitType::Engineer, 1, 150.0f, 0.0f);
        battle.Get<Unit>(doomed)->health = 5.0f; // one hit from anything
        // No reservation: threat logic picks the tank (power 20 > 2).
        CC_CHECK(AcquireTarget(battle, hunter) == doomed);
        // 10 power committed mid-windup: hunter spreads to the engineer.
        ReservedDamageMap reserved;
        reserved[doomed] = 10.0f;
        CC_CHECK(AcquireTarget(battle, hunter, nullptr, &reserved) == healthy);
        // Partial reservation below lethal: still a valid target.
        reserved[doomed] = 4.0f;
        CC_CHECK(AcquireTarget(battle, hunter, nullptr, &reserved) == doomed);
        // Null map (legacy callers, direct UpdateUnit tests): unchanged.
        CC_CHECK(AcquireTarget(battle, hunter, nullptr, nullptr) == doomed);
    }
}
