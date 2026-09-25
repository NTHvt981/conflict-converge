// Unit tests for UnitFactory (costs, funds validation, lifecycle events).

#include "test_harness.h"

#include "Event.h"
#include "Extensions.h"
#include "ResourceSystem.h"
#include "UnitConfig.h"
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
    CC_CHECK(FindCombatState(registry, id)->target == kInvalidEntity);

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

    // --- M2 spawn: gimmick units get extension components, others don't ---
    {
        ResetActiveUnitConfigs();
        Registry reg;
        ResourceSystem res;
        res.AddIron(10000);
        res.AddOil(10000);
        EventDispatcher ev;
        UnitFactory fac(reg, res, ev);
        const Entity heavy = fac.SpawnPrepaid(UnitType::HeavyTank, 0, { 0.0f, 0.0f });
        CC_CHECK(heavy != kInvalidEntity);
        CC_CHECK(reg.Has<Turret>(heavy)); // G1: heavies are turreted by default
        CC_CHECK(reg.Get<Turret>(heavy)->turnRate == 3.0f);
        CC_CHECK(!reg.Has<Cargo>(heavy));
        const Entity rifle = fac.SpawnPrepaid(UnitType::RifleInfantry, 0, { 64.0f, 64.0f });
        CC_CHECK(!reg.Has<Turret>(rifle) && !reg.Has<Cargo>(rifle));

        std::vector<UnitConfig> override;
        UnitConfig ifv = DefaultUnitConfig(UnitType::IFV);
        ifv.abilities.transportCapacity = 4;
        override.push_back(ifv);
        UnitConfig light = DefaultUnitConfig(UnitType::LightTank);
        light.abilities.turretTurnRate = 3.0f;
        override.push_back(light);
        SetActiveUnitConfigs(override);
        const Entity carrier = fac.SpawnPrepaid(UnitType::IFV, 0, { 64.0f, 0.0f });
        CC_CHECK(reg.Has<Cargo>(carrier));
        CC_CHECK(reg.Get<Cargo>(carrier)->capacity == 4);
        const Entity turreted = fac.SpawnPrepaid(UnitType::LightTank, 0, { 128.0f, 0.0f });
        CC_CHECK(reg.Has<Turret>(turreted));
        CC_CHECK(reg.Get<Turret>(turreted)->turnRate == 3.0f);
        ResetActiveUnitConfigs();
    }

    // --- M2 destroy: passengers eject at the carrier's tile ---
    {
        Registry reg;
        Unit hull;
        hull.type = UnitType::IFV;
        ApplyBaseStats(hull);
        hull.teamID = 0;
        hull.position = { 128.0f, 128.0f };
        const Entity carrier = reg.Create();
        reg.Add(carrier, hull);
        Cargo cargo;
        cargo.capacity = 4;
        reg.Add(carrier, cargo);
        Unit rider;
        rider.type = UnitType::RifleInfantry;
        ApplyBaseStats(rider);
        rider.teamID = 0;
        const Entity passenger = reg.Create();
        reg.Add(passenger, rider);
        EmbarkedOn ride;
        ride.carrier = carrier;
        reg.Add(passenger, ride);
        reg.Get<Cargo>(carrier)->passengers.push_back(passenger);

        ResourceSystem res;
        EventDispatcher ev;
        UnitFactory fac(reg, res, ev);
        fac.DestroyUnit(carrier);
        CC_CHECK(!reg.IsAlive(carrier));
        CC_CHECK(reg.IsAlive(passenger));
        CC_CHECK(!reg.Has<EmbarkedOn>(passenger));
        CC_CHECK(reg.Get<Unit>(passenger)->position.x == 128.0f);
    }
}
