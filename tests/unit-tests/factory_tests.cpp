// Unit tests for UnitFactory (costs, funds validation, lifecycle events).

#include "test_harness.h"

#include "Event.h"
#include "ResourceSystem.h"
#include "UnitFactory.h"
#include "UnitStats.h"

void RunFactoryTests()
{
    // --- price list sanity ---
    const UnitCost rifle = CostOf(UnitType::RifleInfantry);
    const UnitCost heavy = CostOf(UnitType::HeavyTank);
    CC_CHECK(rifle.iron >= 0 && rifle.oil >= 0);
    CC_CHECK(heavy.iron > rifle.iron);
    for (int i = 0; i < static_cast<int>(UnitType::Count); ++i)
    {
        const UnitCost cost = CostOf(static_cast<UnitType>(i));
        CC_CHECK(cost.iron >= 0 && cost.oil >= 0);
    }

    // --- successful spawn: spends, stats, snaps, announces ---
    Registry registry;
    ResourceSystem resources;
    resources.AddIron(1000);
    resources.AddOil(500);
    EventDispatcher events;
    int spawned = 0;
    events.Subscribe(EventType::UnitSpawned, [&](const Event &) { ++spawned; });
    UnitFactory factory(registry, resources, events);

    const Entity id = factory.Spawn(UnitType::RifleInfantry, 2, { 70.0f, 130.0f });
    CC_CHECK(id != kInvalidEntity);
    CC_CHECK(spawned == 1);
    CC_CHECK(resources.iron == 1000 - rifle.iron);
    CC_CHECK(resources.oil == 500 - rifle.oil);

    const Unit *unit = registry.Get<Unit>(id);
    CC_CHECK(unit != nullptr);
    CC_CHECK(unit->health == BaseStats(UnitType::RifleInfantry).health);
    CC_CHECK(unit->teamID == 2);
    CC_CHECK(unit->state == UnitState::Idle);
    CC_CHECK(unit->position.x == 64.0f && unit->position.y == 128.0f);
    CC_CHECK(unit->target == kInvalidEntity);

    // --- insufficient funds: nothing spent, created, or announced ---
    ResourceSystem broke;
    EventDispatcher brokeEvents;
    int brokeSpawned = 0;
    brokeEvents.Subscribe(EventType::UnitSpawned, [&](const Event &) { ++brokeSpawned; });
    UnitFactory brokeFactory(registry, broke, brokeEvents);
    const std::size_t before = registry.EntityCount();
    CC_CHECK(brokeFactory.Spawn(UnitType::HeavyTank, 0, { 0.0f, 0.0f }) == kInvalidEntity);
    CC_CHECK(registry.EntityCount() == before);
    CC_CHECK(brokeSpawned == 0);
    CC_CHECK(broke.iron == 0 && broke.oil == 0);

    // --- exact funds succeed and drain to zero ---
    ResourceSystem exact;
    exact.AddIron(heavy.iron);
    exact.AddOil(heavy.oil);
    EventDispatcher exactEvents;
    UnitFactory exactFactory(registry, exact, exactEvents);
    CC_CHECK(exactFactory.Spawn(UnitType::HeavyTank, 1, { 0.0f, 0.0f }) != kInvalidEntity);
    CC_CHECK(exact.iron == 0 && exact.oil == 0);

    // --- destroy: announces once, removes; repeat is a silent no-op ---
    int destroyed = 0;
    events.Subscribe(EventType::UnitDestroyed, [&](const Event &) { ++destroyed; });
    factory.DestroyUnit(id);
    CC_CHECK(destroyed == 1);
    CC_CHECK(!registry.IsAlive(id));
    factory.DestroyUnit(id);
    CC_CHECK(destroyed == 1);
    factory.DestroyUnit(kInvalidEntity);
    CC_CHECK(destroyed == 1);
}
