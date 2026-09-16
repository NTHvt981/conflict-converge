// Unit tests for QoL shift-queued order chains (enqueue, FIFO dequeue on
// completion, patrol/cancel semantics, idle-immediate).

#include "test_harness.h"

#include "TileMap.h"
#include "Unit.h"

namespace
{

Entity SpawnInfantry(Registry &registry, int tileX, int tileY)
{
    Entity entity = registry.Create();
    Unit unit;
    unit.type = UnitType::Infantry;
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
}
