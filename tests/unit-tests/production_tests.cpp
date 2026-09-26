// Unit tests for production queue (costs, progress, prepaid spawn).

#include "test_harness.h"

#include "core/Event.h"
#include "economy/Production.h"
#include "economy/ResourceSystem.h"
#include "units/UnitFactory.h"
#include "units/UnitStats.h"

namespace
{

struct Rig
{
    Registry registry;
    ResourceSystem resources;
    EventDispatcher events;
    UnitFactory factory;
    int spawned = 0;

    Rig() : factory(registry, resources, events)
    {
        resources.AddIron(1000);
        resources.AddOil(500);
        events.Subscribe(EventType::UnitSpawned, [&](const Event &) { ++spawned; });
    }
};

} // namespace

void RunProductionTests()
{
    // --- build times scale with cost, floored at 2s ---
    CC_CHECK(BuildTime(UnitType::RifleInfantry) >= 2.0f);
    CC_CHECK(BuildTime(UnitType::HeavyTank) > BuildTime(UnitType::RifleInfantry));

    // --- enqueue charges upfront; broke orders rejected ---
    Rig rig;
    ProductionQueue queue;
    CC_CHECK(queue.Empty());
    CC_CHECK(queue.HeadProgress() == 0.0f);
    CC_CHECK(queue.Enqueue(rig.resources, UnitType::RifleInfantry));
    const UnitCost cost = CostOf(UnitType::RifleInfantry);
    CC_CHECK(rig.resources.iron == 1000 - cost.iron);
    CC_CHECK(rig.resources.oil == 500 - cost.oil);
    CC_CHECK(queue.Size() == 1);

    ResourceSystem broke;
    ProductionQueue brokeQueue;
    CC_CHECK(!brokeQueue.Enqueue(broke, UnitType::HeavyTank));
    CC_CHECK(brokeQueue.Empty());

    // --- partial progress advances without spawning ---
    queue.Update(rig.factory, rig.resources, 0, { 0.0f, 0.0f },
                 BuildTime(UnitType::RifleInfantry) * 0.5f);
    CC_CHECK(queue.Size() == 1);
    CC_CHECK(rig.spawned == 0);
    const float half = queue.HeadProgress();
    CC_CHECK(half > 0.4f && half < 0.6f);

    // --- completion spawns prepaid at the rally point (no second charge) ---
    const long ironBefore = rig.resources.iron;
    queue.Update(rig.factory, rig.resources, 3, cc::ToRaylib(cc::TileToWorld(4, 4)),
                 BuildTime(UnitType::RifleInfantry));
    CC_CHECK(queue.Empty());
    CC_CHECK(rig.spawned == 1);
    CC_CHECK(rig.resources.iron == ironBefore); // prepaid: balances untouched
    bool found = false;
    rig.registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.teamID == 3 && unit.position.x == 256.0f && unit.position.y == 256.0f)
        {
            found = true;
        }
    });
    CC_CHECK(found);

    // --- items build strictly in order ---
    Rig line;
    ProductionQueue lineQueue;
    CC_CHECK(lineQueue.Enqueue(line.resources, UnitType::RifleInfantry));
    CC_CHECK(lineQueue.Enqueue(line.resources, UnitType::Engineer));
    lineQueue.Update(line.factory, line.resources, 0, { 0.0f, 0.0f }, 1000.0f); // finishes head only
    CC_CHECK(lineQueue.Size() == 1);
    CC_CHECK(line.spawned == 1);
    lineQueue.Update(line.factory, line.resources, 0, { 0.0f, 0.0f }, 1000.0f);
    CC_CHECK(lineQueue.Empty());
    CC_CHECK(line.spawned == 2);

    // --- repeat items re-charge and restart instead of popping ---
    Rig rep;
    ProductionQueue repQueue;
    CC_CHECK(repQueue.Enqueue(rep.resources, UnitType::RifleInfantry, true));
    const UnitCost repCost = CostOf(UnitType::RifleInfantry);
    const long afterFirstCharge = rep.resources.iron;
    repQueue.Update(rep.factory, rep.resources, 0, { 0.0f, 0.0f },
                    BuildTime(UnitType::RifleInfantry));
    CC_CHECK(repQueue.Size() == 1); // still queued, rebuilding
    CC_CHECK(rep.spawned == 1);
    CC_CHECK(rep.resources.iron == afterFirstCharge - repCost.iron); // charged twice
    repQueue.Update(rep.factory, rep.resources, 0, { 0.0f, 0.0f },
                    BuildTime(UnitType::RifleInfantry));
    CC_CHECK(repQueue.Size() == 1);
    CC_CHECK(rep.spawned == 2);

    // --- broke repeat parks at 100% and resumes when funded ---
    Rig poor;
    ProductionQueue poorQueue;
    CC_CHECK(poorQueue.Enqueue(poor.resources, UnitType::RifleInfantry, true));
    poorQueue.Update(poor.factory, poor.resources, 0, { 0.0f, 0.0f },
                     BuildTime(UnitType::RifleInfantry)); // first cycle ok
    CC_CHECK(poor.spawned == 1);
    poor.resources.iron = 0; // drain before the second cycle completes
    poor.resources.oil = 0;
    poorQueue.Update(poor.factory, poor.resources, 0, { 0.0f, 0.0f },
                     BuildTime(UnitType::RifleInfantry));
    CC_CHECK(poorQueue.Size() == 1); // parked, not dropped
    CC_CHECK(poor.spawned == 1);     // no free spawn while broke
    poor.resources.AddIron(1000);
    poor.resources.AddOil(500);
    poorQueue.Update(poor.factory, poor.resources, 0, { 0.0f, 0.0f }, 0.01f);
    CC_CHECK(poor.spawned == 2); // resumes on the next tick once affordable

    // --- cancel still refunds + removes repeat items ("until cancelled") ---
    CC_CHECK(!repQueue.Empty());
    const long beforeCancel = rep.resources.iron;
    repQueue.CancelTop(rep.resources);
    CC_CHECK(repQueue.Empty());
    CC_CHECK(rep.resources.iron == beforeCancel + repCost.iron);

    // --- cancel refunds the head and drops it ---
    Rig refund;
    ProductionQueue refundQueue;
    CC_CHECK(refundQueue.Enqueue(refund.resources, UnitType::LightTank));
    refundQueue.CancelTop(refund.resources);
    CC_CHECK(refundQueue.Empty());
    CC_CHECK(refund.resources.iron == 1000);
    CC_CHECK(refund.resources.oil == 500);
    refundQueue.CancelTop(refund.resources); // empty cancel: safe no-op
    CC_CHECK(refund.resources.iron == 1000);

    // --- SpawnPrepaid never charges, even when broke ---
    ResourceSystem empty;
    EventDispatcher events;
    Registry registry;
    UnitFactory freeFactory(registry, empty, events);
    CC_CHECK(freeFactory.SpawnPrepaid(UnitType::HeavyTank, 1, { 64.0f, 64.0f }) !=
             kInvalidEntity);
    CC_CHECK(empty.iron == 0 && empty.oil == 0);
}
