// Unit tests for M3 Goal 4: threat-priority acquisition and range checks.

#include "test_harness.h"

#include "Targeting.h"

#include "UnitStats.h" // ApplyBaseStats for realistic power/sight values

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

    // --- range check with Infantry (range 128) ---
    Unit attacker;
    attacker.type = UnitType::Infantry;
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
    const Entity seeker = AddSoldier(registry, UnitType::Infantry, 0, 0.0f, 0.0f);
    const Entity weakNear = AddSoldier(registry, UnitType::Engineer, 1, 100.0f, 0.0f); // pow 2
    const Entity strongFar = AddSoldier(registry, UnitType::LightTank, 1, 200.0f, 0.0f); // pow 20
    CC_CHECK(AcquireTarget(registry, seeker) == strongFar);

    // --- equal threat: nearest wins ---
    registry.Get<Unit>(strongFar)->attackPower = 2;
    CC_CHECK(AcquireTarget(registry, seeker) == weakNear);

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
    blind.sightRange = 0.0f;
    const Entity blindId = registry.Create();
    registry.Add(blindId, blind);
    CC_CHECK(AcquireTarget(registry, blindId) == kInvalidEntity);
}
