// Unit tests for QoL shift-queued order chains (enqueue, FIFO dequeue on
// completion, patrol/cancel semantics, idle-immediate).

#include "test_harness.h"

#include "units/Extensions.h"
#include "units/Formation.h"
#include "world/Pathfinder.h" // OccupancyGrid for the footprint-aware order paths
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

namespace
{

Entity SpawnInfantry(Registry &registry, int tileX, int tileY)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.type = UnitType::RifleInfantry;
    unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
    unit.speed = 64.0f; // 1 tile per 1s tick below
    unit.sightRange = 0.0f; // blind: attack-move marches never engage
    registry.Add(entity, unit);
    registry.Add(entity, Orders{});
    registry.Add(entity, Mover{});
    return entity;
}

Vector2 TilePos(int tileX, int tileY)
{
    return cc::ToRaylib(cc::TileToWorld(tileX, tileY));
}

// Walk frames until `unit` is attack-moving (or the budget runs out).
void WalkUntilAttackMove(Entity id, Registry &registry, TileMap &map, int maxFrames)
{
    for (int i = 0; i < maxFrames; ++i)
    {
        UpdateUnit(id, registry, map, 1.0f);
        if (GetOrders(registry, id).attackMove)
        {
            return;
        }
    }
}

} // namespace

void RunOrderQueueTests()
{
    // --- queueing behind a live order ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);

        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, false,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 1) });
        CC_CHECK(FindMover(registry, id)->hasMoveOrder);
        CC_CHECK(orders.orderQueue.empty());

        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::AttackMove, TilePos(8, 8) });
        CC_CHECK(orders.orderQueue.size() == 1);
        CC_CHECK(!orders.attackMove); // still on the first order

        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Patrol, TilePos(8, 8), TilePos(1, 8) });
        CC_CHECK(orders.orderQueue.size() == 2);

        // First order completes -> attack-move dequeues and starts.
        WalkUntilAttackMove(id, registry, map, 30);
        CC_CHECK(orders.attackMove);
        CC_CHECK(orders.orderQueue.size() == 1);

        // March completes (no enemies: sightRange 0) -> patrol takes over.
        for (int i = 0; i < 60 && !orders.hasPatrol; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(orders.hasPatrol);
        CC_CHECK(orders.orderQueue.empty());
    }

    // --- queueing on an idle unit issues immediately ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);

        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(5, 5) });
        CC_CHECK(FindMover(registry, id)->hasMoveOrder);
        CC_CHECK(orders.orderQueue.empty());
    }

    // --- non-queued issue replaces the whole queue ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);

        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, false,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 1) });
        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(1, 8) });
        CC_CHECK(orders.orderQueue.size() == 1);
        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, false,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 8) });
        CC_CHECK(orders.orderQueue.empty());
    }

    // --- patrol legs never dequeue what waits behind them ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);

        IssuePatrolOrder(unit, orders, GetMover(registry, id), map, TilePos(1, 1), TilePos(3, 1));
        orders.orderQueue.push_back(QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 8) });
        for (int i = 0; i < 20; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(orders.hasPatrol); // still looping legs
        CC_CHECK(orders.orderQueue.size() == 1); // nothing dequeued
    }

    // --- blocked cancel drops the rest of the queue ---
    {
        Registry registry;
        TileMap map(10, 10);
        map.Set({ 2, 1 }, TerrainType::Water);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);

        IssueMoveOrder(unit, orders, GetMover(registry, id), TilePos(8, 1)); // straight at the water, no path
        IssueOrEnqueue(unit, orders, GetMover(registry, id), map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(1, 8) });
        CC_CHECK(orders.orderQueue.size() == 1);
        UpdateUnit(id, registry, map, 1.0f); // steps into water -> cancels
        CC_CHECK(!FindMover(registry, id)->hasMoveOrder);
        CC_CHECK(orders.orderQueue.empty()); // stuck: rest dropped, not marched
    }

    // --- repair orders queue and complete like other order types ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity eng = SpawnInfantry(registry, 1, 1);
        registry.Get<Unit>(eng)->type = UnitType::Engineer;
        const Entity tank = SpawnInfantry(registry, 2, 1);
        Unit *patient = registry.Get<Unit>(tank);
        patient->type = UnitType::LightTank;
        const float maxHp = BaseStats(UnitType::LightTank).health;
        patient->health = maxHp - 30.0f; // damaged but alive
        Unit &engineer = *registry.Get<Unit>(eng);
        Orders &engOrders = GetOrders(registry, eng);

        // Pre-checks mirror the right-click gesture's gating.
        CC_CHECK(CanRepairTarget(registry, engineer, tank));
        CC_CHECK(!CanRepairTarget(registry, *patient, tank)); // not an Engineer
        patient->health = maxHp;
        CC_CHECK(!CanRepairTarget(registry, engineer, tank)); // healthy: nothing to do
        patient->health = maxHp - 30.0f;

        IssueOrEnqueue(engineer, engOrders, GetMover(registry, eng), map, nullptr, eng, 0, false,
                       QueuedOrder{ QueuedOrderKind::Repair, {}, {}, tank });
        CC_CHECK(engOrders.hasRepairOrder);
        for (int i = 0; i < 30 && engOrders.hasRepairOrder; ++i)
        {
            UpdateUnit(eng, registry, map, 1.0f);
        }
        CC_CHECK(!engOrders.hasRepairOrder); // healed to full: order done
        CC_CHECK(patient->health == maxHp);
    }

    // --- Bugfix: fresh single move clears a stale repair order ---
    {
        Registry registry;
        TileMap map(10, 10);
        OccupancyGrid occ(10, 10);
        const Entity eng = SpawnInfantry(registry, 1, 1);
        registry.Get<Unit>(eng)->type = UnitType::Engineer;
        const Entity tank = SpawnInfantry(registry, 2, 1);
        Unit *patient = registry.Get<Unit>(tank);
        patient->type = UnitType::LightTank;
        patient->health = BaseStats(UnitType::LightTank).health - 30.0f;
        Unit &engineer = *registry.Get<Unit>(eng);
        Orders &engOrders = GetOrders(registry, eng);
        IssueRepairOrder(engineer, engOrders, GetMover(registry, eng), tank);
        CC_CHECK(engOrders.hasRepairOrder);

        // Same sequence as Game.cpp's single-unit right-click branch.
        engOrders.orderQueue.clear();
        ClearOrders(engineer, engOrders, GetMover(registry, eng));
        IssuePathOrderFootprint(engineer, engOrders, GetMover(registry, eng), map, occ,
                                TilePos(8, 8), eng, registry.Generation(eng));
        CC_CHECK(!engOrders.hasRepairOrder);
        CC_CHECK(engOrders.repairTarget == kInvalidEntity);
        CC_CHECK(FindMover(registry, eng)->hasMoveOrder || FindMover(registry, eng)->hasPath);
    }

    // --- Bugfix: squad formation move clears a stale attack-ground order ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        std::vector<Entity> squad;
        for (int i = 0; i < 2; ++i)
        {
            squad.push_back(SpawnInfantry(registry, i, 0));
        }
        Unit &lead = *registry.Get<Unit>(squad[0]);
        Orders &leadOrders = GetOrders(registry, squad[0]);
        IssueAttackGroundOrder(lead, leadOrders, GetMover(registry, squad[0]), map, TilePos(5, 5));
        CC_CHECK(leadOrders.hasAttackGroundOrder);

        formation::IssueFormationMoveFP(registry, squad, map, occ, TilePos(15, 10),
                                        false);
        CC_CHECK(!leadOrders.hasAttackGroundOrder);
        CC_CHECK(FindMover(registry, squad[0])->hasMoveOrder ||
                 FindMover(registry, squad[0])->hasPath);
    }

    // --- Bugfix: line formation move clears a stale patrol (no resume) ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity id = SpawnInfantry(registry, 1, 1);
        const std::vector<Entity> squad = { id };
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);
        IssuePatrolOrder(unit, orders, GetMover(registry, id), map, TilePos(1, 1), TilePos(3, 1));
        CC_CHECK(orders.hasPatrol);

        formation::IssueLineFormationMoveFP(registry, squad, map, occ, TilePos(10, 10),
                                            TilePos(12, 10), false);
        CC_CHECK(!orders.hasPatrol);
        CC_CHECK(FindMover(registry, id)->hasMoveOrder || FindMover(registry, id)->hasPath);
        // Walk to arrival: must stop, not resume the old patrol route.
        for (int i = 0; i < 60; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(!orders.hasPatrol);
    }

    // --- Bugfix: line formation move drops a queued backlog ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity id = SpawnInfantry(registry, 1, 1);
        const std::vector<Entity> squad = { id };
        Unit &unit = *registry.Get<Unit>(id);
        Orders &orders = GetOrders(registry, id);
        orders.orderQueue.push_back(QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 8) });

        formation::IssueLineFormationMoveFP(registry, squad, map, occ, TilePos(10, 10),
                                            TilePos(12, 10), false);
        CC_CHECK(orders.orderQueue.empty());
    }
}
