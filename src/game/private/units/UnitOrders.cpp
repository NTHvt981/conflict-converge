
#include "units/Unit.h"

#include "units/UnitInternal.h"
#include "world/Pathfinder.h"

OrderContext MakeOrderContext(Registry &registry, Entity self, const TileMap &map,
							  const OccupancyGrid *occ)
{
	return OrderContext{ GetOrders(registry, self), GetMover(registry, self), map, occ, self,
						 registry.Generation(self) };
}

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

void IssueAttackMoveOrderFootprint(Unit &unit, const OrderContext &ctx, Vector2 worldTarget)
{
	ClearOrders(unit, ctx.orders, ctx.mover);
	IssuePathOrderFootprint(unit, ctx, worldTarget);
	ctx.orders.attackMove = true;
	ctx.orders.attackMoveDest = ctx.mover.moveTarget;
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
	mover.yieldRequestBlocker = kInvalidEntity;
	mover.yieldRequestTile = { -1, -1 };
	mover.awaitingClearBlocker = kInvalidEntity;
	mover.awaitingClearTile = { -1, -1 };
	mover.awaitingClearTime = 0.0f;
	mover.yieldDeclined = kInvalidEntity;
}

void IssueAttackGroundOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
							Vector2 worldPos)
{
	ClearOrders(unit, orders, mover);
	orders.hasAttackGroundOrder = true;
	orders.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
	IssuePathOrder(unit, orders, mover, map, orders.attackGroundPos);
}

void IssueAttackGroundOrderFootprint(Unit &unit, const OrderContext &ctx, Vector2 worldPos)
{
	ClearOrders(unit, ctx.orders, ctx.mover);
	ctx.orders.hasAttackGroundOrder = true;
	ctx.orders.attackGroundPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
	IssuePathOrderFootprint(unit, ctx, ctx.orders.attackGroundPos);
}

namespace
{

void DispatchQueuedOrder(Unit &unit, const OrderContext &ctx, const QueuedOrder &order)
{
	ClearOrders(unit, ctx.orders, ctx.mover);
	switch (order.kind)
	{
	case QueuedOrderKind::Move:
		if (ctx.occ != nullptr)
		{
			IssuePathOrderFootprint(unit, ctx, order.pointA);
		}
		else
		{
			IssuePathOrder(unit, ctx.orders, ctx.mover, ctx.map, order.pointA);
		}
		break;
	case QueuedOrderKind::AttackMove:
		if (ctx.occ != nullptr)
		{
			IssueAttackMoveOrderFootprint(unit, ctx, order.pointA);
		}
		else
		{
			IssueAttackMoveOrder(unit, ctx.orders, ctx.mover, ctx.map, order.pointA);
		}
		break;
	case QueuedOrderKind::Patrol:
		IssuePatrolOrder(unit, ctx.orders, ctx.mover, ctx.map, order.pointA, order.pointB);
		break;
	case QueuedOrderKind::Repair:
		if (unit.type == UnitType::Medic)
		{
			IssueHealOrder(unit, ctx.orders, ctx.mover, order.target);
		}
		else
		{
			IssueRepairOrder(unit, ctx.orders, ctx.mover, order.target);
		}
		break;
	case QueuedOrderKind::AttackGround:
		if (ctx.occ != nullptr)
		{
			IssueAttackGroundOrderFootprint(unit, ctx, order.pointA);
		}
		else
		{
			IssueAttackGroundOrder(unit, ctx.orders, ctx.mover, ctx.map, order.pointA);
		}
		break;
	case QueuedOrderKind::Load:
		IssueLoadOrder(unit, ctx.orders, ctx.mover, order.target);
		break;
	case QueuedOrderKind::Unload:
		IssueUnloadOrder(unit, ctx.orders, ctx.mover, ctx.map, order.pointA);
		break;
	case QueuedOrderKind::Count:
		break;
	}
}

}

void OnOrderFinished(Unit &unit, const OrderContext &ctx)
{
	if (ctx.orders.hasPatrol || ctx.orders.orderQueue.empty())
	{
		return;
	}
	const QueuedOrder next = ctx.orders.orderQueue.front();
	ctx.orders.orderQueue.erase(ctx.orders.orderQueue.begin());
	DispatchQueuedOrder(unit, ctx, next);
}

void OnOrderCancelled(Orders &orders)
{
	orders.orderQueue.clear();
}

void IssueOrEnqueue(Unit &unit, const OrderContext &ctx, bool shiftQueue, QueuedOrder order)
{
	if (!shiftQueue)
	{
		ctx.orders.orderQueue.clear();
		DispatchQueuedOrder(unit, ctx, order);
		return;
	}
	if (!ctx.mover.hasMoveOrder && !ctx.mover.hasPath && !ctx.orders.hasRepairOrder &&
		!ctx.orders.hasLoadOrder && !ctx.orders.hasUnloadOrder && !ctx.orders.hasPatrol &&
		ctx.orders.orderQueue.empty())
	{
		DispatchQueuedOrder(unit, ctx, order);
		return;
	}
	ctx.orders.orderQueue.push_back(order);
}
