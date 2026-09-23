// Unit tests for QoL shift-queued order chains (enqueue, FIFO dequeue on
// completion, patrol/cancel semantics, idle-immediate).

#include "test_harness.h"

#include "Formation.h"
#include "Pathfinder.h" // OccupancyGrid for the footprint-aware order paths
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"

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
        if (registry.Get<Unit>(id)->attackMove)
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

        IssueOrEnqueue(unit, map, nullptr, id, 0, false,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 1) });
        CC_CHECK(unit.hasMoveOrder);
        CC_CHECK(unit.orderQueue.empty());

        IssueOrEnqueue(unit, map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::AttackMove, TilePos(8, 8) });
        CC_CHECK(unit.orderQueue.size() == 1);
        CC_CHECK(!unit.attackMove); // still on the first order

        IssueOrEnqueue(unit, map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Patrol, TilePos(8, 8), TilePos(1, 8) });
        CC_CHECK(unit.orderQueue.size() == 2);

        // First order completes -> attack-move dequeues and starts.
        WalkUntilAttackMove(id, registry, map, 30);
        CC_CHECK(unit.attackMove);
        CC_CHECK(unit.orderQueue.size() == 1);

        // March completes (no enemies: sightRange 0) -> patrol takes over.
        for (int i = 0; i < 60 && !unit.hasPatrol; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(unit.hasPatrol);
        CC_CHECK(unit.orderQueue.empty());
    }

    // --- queueing on an idle unit issues immediately ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);

        IssueOrEnqueue(unit, map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(5, 5) });
        CC_CHECK(unit.hasMoveOrder);
        CC_CHECK(unit.orderQueue.empty());
    }

    // --- non-queued issue replaces the whole queue ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);

        IssueOrEnqueue(unit, map, nullptr, id, 0, false,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 1) });
        IssueOrEnqueue(unit, map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(1, 8) });
        CC_CHECK(unit.orderQueue.size() == 1);
        IssueOrEnqueue(unit, map, nullptr, id, 0, false,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 8) });
        CC_CHECK(unit.orderQueue.empty());
    }

    // --- patrol legs never dequeue what waits behind them ---
    {
        Registry registry;
        TileMap map(10, 10);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);

        IssuePatrolOrder(unit, map, TilePos(1, 1), TilePos(3, 1));
        unit.orderQueue.push_back(QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 8) });
        for (int i = 0; i < 20; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(unit.hasPatrol); // still looping legs
        CC_CHECK(unit.orderQueue.size() == 1); // nothing dequeued
    }

    // --- blocked cancel drops the rest of the queue ---
    {
        Registry registry;
        TileMap map(10, 10);
        map.Set({ 2, 1 }, TerrainType::Water);
        const Entity id = SpawnInfantry(registry, 1, 1);
        Unit &unit = *registry.Get<Unit>(id);

        IssueMoveOrder(unit, TilePos(8, 1)); // straight at the water, no path
        IssueOrEnqueue(unit, map, nullptr, id, 0, true,
                       QueuedOrder{ QueuedOrderKind::Move, TilePos(1, 8) });
        CC_CHECK(unit.orderQueue.size() == 1);
        UpdateUnit(id, registry, map, 1.0f); // steps into water -> cancels
        CC_CHECK(!unit.hasMoveOrder);
        CC_CHECK(unit.orderQueue.empty()); // stuck: rest dropped, not marched
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

        // Pre-checks mirror the right-click gesture's gating.
        CC_CHECK(CanRepairTarget(registry, engineer, tank));
        CC_CHECK(!CanRepairTarget(registry, *patient, tank)); // not an Engineer
        patient->health = maxHp;
        CC_CHECK(!CanRepairTarget(registry, engineer, tank)); // healthy: nothing to do
        patient->health = maxHp - 30.0f;

        IssueOrEnqueue(engineer, map, nullptr, eng, 0, false,
                       QueuedOrder{ QueuedOrderKind::Repair, {}, {}, tank });
        CC_CHECK(engineer.hasRepairOrder);
        for (int i = 0; i < 30 && engineer.hasRepairOrder; ++i)
        {
            UpdateUnit(eng, registry, map, 1.0f);
        }
        CC_CHECK(!engineer.hasRepairOrder); // healed to full: order done
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
        IssueRepairOrder(engineer, tank);
        CC_CHECK(engineer.hasRepairOrder);

        // Same sequence as Game.cpp's single-unit right-click branch.
        engineer.orderQueue.clear();
        ClearOrders(engineer);
        IssuePathOrderFootprint(engineer, map, occ, TilePos(8, 8), eng,
                                registry.Generation(eng));
        CC_CHECK(!engineer.hasRepairOrder);
        CC_CHECK(engineer.repairTarget == kInvalidEntity);
        CC_CHECK(engineer.hasMoveOrder || engineer.hasPath);
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
        IssueAttackGroundOrder(lead, map, TilePos(5, 5));
        CC_CHECK(lead.hasAttackGroundOrder);

        formation::IssueFormationMoveFP(registry, squad, map, occ, TilePos(15, 10),
                                        false);
        CC_CHECK(!lead.hasAttackGroundOrder);
        CC_CHECK(lead.hasMoveOrder || lead.hasPath);
    }

    // --- Bugfix: line formation move clears a stale patrol (no resume) ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity id = SpawnInfantry(registry, 1, 1);
        const std::vector<Entity> squad = { id };
        Unit &unit = *registry.Get<Unit>(id);
        IssuePatrolOrder(unit, map, TilePos(1, 1), TilePos(3, 1));
        CC_CHECK(unit.hasPatrol);

        formation::IssueLineFormationMoveFP(registry, squad, map, occ, TilePos(10, 10),
                                            TilePos(12, 10), false);
        CC_CHECK(!unit.hasPatrol);
        CC_CHECK(unit.hasMoveOrder || unit.hasPath);
        // Walk to arrival: must stop, not resume the old patrol route.
        for (int i = 0; i < 60; ++i)
        {
            UpdateUnit(id, registry, map, 1.0f);
        }
        CC_CHECK(!unit.hasPatrol);
    }

    // --- Bugfix: line formation move drops a queued backlog ---
    {
        Registry registry;
        TileMap map(20, 15);
        OccupancyGrid occ(20, 15);
        const Entity id = SpawnInfantry(registry, 1, 1);
        const std::vector<Entity> squad = { id };
        Unit &unit = *registry.Get<Unit>(id);
        unit.orderQueue.push_back(QueuedOrder{ QueuedOrderKind::Move, TilePos(8, 8) });

        formation::IssueLineFormationMoveFP(registry, squad, map, occ, TilePos(10, 10),
                                            TilePos(12, 10), false);
        CC_CHECK(unit.orderQueue.empty());
    }
}
