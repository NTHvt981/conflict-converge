
#include "units/Unit.h"

#include "units/UnitInternal.h"
#include "world/Pathfinder.h"

void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 worldTarget)
{
    mover.moveTarget = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldTarget)));
    mover.hasMoveOrder = true;
    mover.blockedTime = 0.0f;
    mover.blockedRepaths = 0;
    ClearOrders(unit, orders, mover);
}

void IssueAttackMoveOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                          Vector2 worldTarget)
{
    ClearOrders(unit, orders, mover);
    IssuePathOrder(unit, orders, mover, map, worldTarget);
    orders.attackMove = true;
    orders.attackMoveDest = mover.moveTarget;
}

void IssueAttackMoveOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                                   const OccupancyGrid &occ, Vector2 worldTarget, Entity self,
                                   std::uint32_t selfGen)
{
    ClearOrders(unit, orders, mover);
    IssuePathOrderFootprint(unit, orders, mover, map, occ, worldTarget, self, selfGen);
    orders.attackMove = true;
    orders.attackMoveDest = mover.moveTarget;
}

void SetStance(Unit &unit, Orders &orders, CombatState &combat, Stance stance)
{
    orders.stance = stance;
    if (stance != Stance::Patrol)
    {
        orders.hasPatrol = false;
    }
    if (stance == Stance::Hold)
    {
        LoseTarget(unit, combat);
    }
}

void IssuePatrolOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map, Vector2 pointA,
                      Vector2 pointB)
{
    ClearOrders(unit, orders, mover);
    orders.stance = Stance::Patrol;
    orders.hasPatrol = true;
    orders.patrolA = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointA)));
    orders.patrolB = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(pointB)));
    orders.patrolToB = true;
    IssuePathOrder(unit, orders, mover, map, orders.patrolB);
}

void IssueRepairOrder(Unit &engineer, Orders &orders, Mover &mover, Entity target)
{
    if (engineer.type != UnitType::Engineer)
    {
        return;
    }
    StopMoving(engineer, mover);
    ClearOrders(engineer, orders, mover);
    orders.hasRepairOrder = true;
    orders.repairTarget = target;
}

void IssueHealOrder(Unit &medic, Orders &orders, Mover &mover, Entity target)
{
    if (medic.type != UnitType::Medic)
    {
        return;
    }
    StopMoving(medic, mover);
    ClearOrders(medic, orders, mover);
    orders.hasRepairOrder = true;
    orders.repairTarget = target;
}

void ClearOrders(Unit &unit, Orders &orders, Mover &mover)
{
    (void)unit;
    orders.attackMove = false;
    orders.hasPatrol = false;
    orders.hasRepairOrder = false;
    orders.repairTarget = kInvalidEntity;
    orders.hasAttackGroundOrder = false;
    orders.hasLoadOrder = false;
    orders.loadTarget = kInvalidEntity;
    orders.hasUnloadOrder = false;
    mover.speedCapPixelsPerSec = -1.0f;
}

void IssueAttackGroundOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                            Vector2 worldPos)
{
    ClearOrders(unit, orders, mover);
    orders.hasAttackGroundOrder = true;
    orders.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
    IssuePathOrder(unit, orders, mover, map, orders.attackGroundPos);
}

void IssueAttackGroundOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                                     const OccupancyGrid &occ, Vector2 worldPos, Entity self,
                                     std::uint32_t selfGen)
{
    ClearOrders(unit, orders, mover);
    orders.hasAttackGroundOrder = true;
    orders.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
    IssuePathOrderFootprint(unit, orders, mover, map, occ, orders.attackGroundPos, self, selfGen);
}

namespace
{

void DispatchQueuedOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                         OccupancyGrid *occ, Entity self, std::uint32_t selfGen,
                         const QueuedOrder &order)
{
    ClearOrders(unit, orders, mover);
    switch (order.kind)
    {
    case QueuedOrderKind::Move:
        if (occ != nullptr)
        {
            IssuePathOrderFootprint(unit, orders, mover, map, *occ, order.pointA, self, selfGen);
        }
        else
        {
            IssuePathOrder(unit, orders, mover, map, order.pointA);
        }
        break;
    case QueuedOrderKind::AttackMove:
        if (occ != nullptr)
        {
            IssueAttackMoveOrderFootprint(unit, orders, mover, map, *occ, order.pointA, self,
                                          selfGen);
        }
        else
        {
            IssueAttackMoveOrder(unit, orders, mover, map, order.pointA);
        }
        break;
    case QueuedOrderKind::Patrol:
        IssuePatrolOrder(unit, orders, mover, map, order.pointA, order.pointB);
        break;
    case QueuedOrderKind::Repair:
        if (unit.type == UnitType::Medic)
        {
            IssueHealOrder(unit, orders, mover, order.target);
        }
        else
        {
            IssueRepairOrder(unit, orders, mover, order.target);
        }
        break;
    case QueuedOrderKind::AttackGround:
        if (occ != nullptr)
        {
            IssueAttackGroundOrderFootprint(unit, orders, mover, map, *occ, order.pointA, self,
                                            selfGen);
        }
        else
        {
            IssueAttackGroundOrder(unit, orders, mover, map, order.pointA);
        }
        break;
    case QueuedOrderKind::Load:
        IssueLoadOrder(unit, orders, mover, order.target);
        break;
    case QueuedOrderKind::Unload:
        IssueUnloadOrder(unit, orders, mover, map, order.pointA);
        break;
    case QueuedOrderKind::Count:
        break;
    }
}

}

void OnOrderFinished(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                     OccupancyGrid *occ, Entity self, std::uint32_t selfGen)
{
    if (orders.hasPatrol || orders.orderQueue.empty())
    {
        return;
    }
    const QueuedOrder next = orders.orderQueue.front();
    orders.orderQueue.erase(orders.orderQueue.begin());
    DispatchQueuedOrder(unit, orders, mover, map, occ, self, selfGen, next);
}

void OnOrderCancelled(Orders &orders)
{
    orders.orderQueue.clear();
}

void IssueOrEnqueue(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                    OccupancyGrid *occ, Entity self, std::uint32_t selfGen, bool shiftQueue,
                    QueuedOrder order)
{
    if (!shiftQueue)
    {
        orders.orderQueue.clear();
        DispatchQueuedOrder(unit, orders, mover, map, occ, self, selfGen, order);
        return;
    }
    if (!mover.hasMoveOrder && !mover.hasPath && !orders.hasRepairOrder && !orders.hasLoadOrder &&
        !orders.hasUnloadOrder && !orders.hasPatrol && orders.orderQueue.empty())
    {
        DispatchQueuedOrder(unit, orders, mover, map, occ, self, selfGen, order);
        return;
    }
    orders.orderQueue.push_back(order);
}
