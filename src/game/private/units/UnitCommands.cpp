#include "units/UnitCommands.h"

#include "units/Extensions.h"

void SetSelectionStance(Registry &registry, Stance stance)
{
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected)
        {
            SetStance(unit, GetOrders(registry, id), GetCombatState(registry, id), stance);
        }
    });
}

void HaltSelection(Registry &registry)
{
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected)
        {
            Orders &orders = GetOrders(registry, id);
            Mover &mover = GetMover(registry, id);
            CombatState &combat = GetCombatState(registry, id);
            mover.hasMoveOrder = false;
            mover.hasPath = false;
            mover.path.clear();
            mover.pathNext = 0;
            combat.target = kInvalidEntity;
            orders.attackMove = false;
            orders.hasRepairOrder = false;
            orders.repairTarget = kInvalidEntity;
            orders.hasAttackGroundOrder = false;
            mover.speedCapPixelsPerSec = -1.0f;
            orders.orderQueue.clear();
            combat.phase = AttackPhase::Ready;
            unit.velocity = { 0.0f, 0.0f };
            unit.state = UnitState::Idle;
            SnapUnitToTile(unit);
        }
    });
}

void ToggleSelectionAutoRetreat(Registry &registry)
{
    bool allOn = true;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.isSelected)
        {
            const Orders *orders = FindOrders(registry, id);
            if (orders == nullptr || !orders->autoRetreat)
            {
                allOn = false;
            }
        }
    });
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected)
        {
            GetOrders(registry, id).autoRetreat = !allOn;
        }
    });
}

void ToggleSelectionAutoRepair(Registry &registry)
{
    bool allOn = true;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.isSelected && unit.type == UnitType::Engineer)
        {
            const Orders *orders = FindOrders(registry, id);
            if (orders == nullptr || !orders->autoRepair)
            {
                allOn = false;
            }
        }
    });
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected && unit.type == UnitType::Engineer)
        {
            GetOrders(registry, id).autoRepair = !allOn;
        }
    });
}

int IssueSelectionAttackMove(Registry &registry, const TileMap &map, OccupancyGrid *occ,
                             Vector2 dest, bool queued)
{
    int acted = 0;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected)
        {
            IssueOrEnqueue(unit, GetOrders(registry, id), GetMover(registry, id), map, occ, id,
                           registry.Generation(id), queued,
                           QueuedOrder{ QueuedOrderKind::AttackMove, dest });
            ++acted;
        }
    });
    return acted;
}

int IssueSelectionPatrol(Registry &registry, const TileMap &map, OccupancyGrid *occ, Vector2 dest,
                         bool queued)
{
    int acted = 0;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected)
        {
            IssueOrEnqueue(unit, GetOrders(registry, id), GetMover(registry, id), map, occ, id,
                           registry.Generation(id), queued,
                           QueuedOrder{ QueuedOrderKind::Patrol, unit.position, dest });
            ++acted;
        }
    });
    return acted;
}

int IssueSelectionRepair(Registry &registry, const TileMap &map, OccupancyGrid *occ,
                         Entity target, bool queued)
{
    int acted = 0;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected && unit.type == UnitType::Engineer &&
            CanRepairTarget(registry, unit, target))
        {
            IssueOrEnqueue(unit, GetOrders(registry, id), GetMover(registry, id), map, occ, id,
                           registry.Generation(id), queued,
                           QueuedOrder{ QueuedOrderKind::Repair, {}, {}, target });
            ++acted;
        }
    });
    return acted;
}

int IssueSelectionHeal(Registry &registry, const TileMap &map, OccupancyGrid *occ, Entity target,
                       bool queued)
{
    int acted = 0;
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        if (unit.isSelected && unit.type == UnitType::Medic &&
            CanHealTarget(registry, unit, target))
        {
            IssueOrEnqueue(unit, GetOrders(registry, id), GetMover(registry, id), map, occ, id,
                           registry.Generation(id), queued,
                           QueuedOrder{ QueuedOrderKind::Repair, {}, {}, target });
            ++acted;
        }
    });
    return acted;
}
