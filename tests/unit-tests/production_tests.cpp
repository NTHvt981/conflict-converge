// Unit tests for M5 Goal 6 production queue (costs, progress, prepaid spawn).

#include "test_harness.h"

#include "Event.h"
#include "Production.h"
#include "ResourceSystem.h"
#include "UnitFactory.h"
#include "UnitStats.h"

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
    CC_CHECK(BuildTime(UnitType::Infantry) >= 2.0f);
    CC_CHECK(BuildTime(UnitType::HeavyTank) > BuildTime(UnitType::Infantry));

    // --- enqueue charges upfront; broke orders rejected ---
    Rig rig;
    ProductionQueue queue;
    CC_CHECK(queue.Empty());
    CC_CHECK(queue.HeadProgress() == 0.0f);
    CC_CHECK(queue.Enqueue(rig.resources, UnitType::Infantry));
    const UnitCost cost = CostOf(UnitType::Infantry);
    CC_CHECK(rig.resources.iron == 1000 - cost.iron);
    CC_CHECK(rig.resources.oil == 500 - cost.oil);
    CC_CHECK(queue.Size() == 1);

    ResourceSystem broke;
    ProductionQueue brokeQueue;
    CC_CHECK(!brokeQueue.Enqueue(broke, UnitType::HeavyTank));
    CC_CHECK(brokeQueue.Empty());

    // --- partial progress advances without spawning ---
    queue.Update(rig.factory, 0, { 0.0f, 0.0f }, BuildTime(UnitType::Infantry) * 0.5f);
    CC_CHECK(queue.Size() == 1);
    CC_CHECK(rig.spawned == 0);
    const float half = queue.HeadProgress();
    CC_CHECK(half > 0.4f && half < 0.6f);

    // --- completion spawns prepaid at the rally point (no second charge) ---
    const long ironBefore = rig.resources.iron;
    queue.Update(rig.factory, 3, cc::ToRaylib(cc::TileToWorld(4, 4)), BuildTime(UnitType::Infantry));
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
    CC_CHECK(lineQueue.Enqueue(line.resources, UnitType::Infantry));
    CC_CHECK(lineQueue.Enqueue(line.resources, UnitType::Engineer));
    lineQueue.Update(line.factory, 0, { 0.0f, 0.0f }, 1000.0f); // finishes head only
    CC_CHECK(lineQueue.Size() == 1);
    CC_CHECK(line.spawned == 1);
    lineQueue.Update(line.factory, 0, { 0.0f, 0.0f }, 1000.0f);
    CC_CHECK(lineQueue.Empty());
    CC_CHECK(line.spawned == 2);

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
