
#include "units/Unit.h"

#include "units/Extensions.h"
#include "core/MathUtils.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/UnitInternal.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{

enum class StepResult
{
	Arrived,
	Blocked,
	BlockedUnit,
	Stepped,
};

StepResult StepToward(cc::Vec2 pos, cc::Vec2 target, float step, const TileMap &map, cc::Vec2 &outNext,
					  const OccupancyGrid *occ = nullptr, Entity self = 0, std::uint32_t selfGen = 0,
					  int footprintW = 1, int footprintH = 1,
					  cc::IVec2 *blockedTile = nullptr)
{
	if (step <= 0.0f)
	{
		return StepResult::BlockedUnit;
	}
	const cc::Vec2 diff = target - pos;
	const float dist = glm::length(diff);
	if (dist <= step)
	{
		// Treat off-map as blocked
		const cc::IVec2 fromTile = cc::WorldToTile(pos);
		const cc::IVec2 targetTile = cc::WorldToTile(target);
		if (targetTile != fromTile && map.IsBlocked(targetTile))
		{
			if (blockedTile)
			{
				*blockedTile = targetTile;
			}
			return StepResult::Blocked;
		}
		if (occ != nullptr && targetTile != fromTile &&
			!occ->CanEnter(map, targetTile, footprintW, footprintH, self, selfGen))
		{
			if (blockedTile)
			{
				*blockedTile = targetTile;
			}
			return StepResult::BlockedUnit;
		}
		outNext = target;
		return StepResult::Arrived;
	}

	const cc::Vec2 next = pos + diff / dist * step;
	const cc::IVec2 from = cc::WorldToTile(pos);
	const cc::IVec2 to = cc::WorldToTile(next);
	if (map.IsBlocked(to) && to != from)
	{
		if (blockedTile)
		{
			*blockedTile = to;
		}
		return StepResult::Blocked;
	}
	if (occ != nullptr && to != from)
	{
		if (!occ->CanEnter(map, to, footprintW, footprintH, self, selfGen))
		{
			if (blockedTile)
			{
				*blockedTile = to;
			}
			return StepResult::BlockedUnit;
		}
	}
	outNext = next;
	return StepResult::Stepped;
}

void Arrive(Unit &unit, Mover &mover, cc::Vec2 where)
{
	unit.position = cc::ToRaylib(where);
	unit.velocity = { 0.0f, 0.0f };
	mover.hasMoveOrder = false;
	mover.hasPath = false;
	mover.path.clear();
	mover.pathNext = 0;
	unit.state = UnitState::Idle;
	mover.blockedTime = 0.0f;
	mover.blockedRepaths = 0;
	mover.speedCapPixelsPerSec = -1.0f;
}

void CancelAtBlocked(Unit &unit, Mover &mover, const TileMap &map)
{
	unit.velocity = { 0.0f, 0.0f };
	mover.hasMoveOrder = false;
	mover.hasPath = false;
	mover.path.clear();
	mover.pathNext = 0;
	unit.state = UnitState::Idle;
	mover.blockedTime = 0.0f;
	mover.blockedRepaths = 0;
	mover.speedCapPixelsPerSec = -1.0f;
	SnapUnitToTile(unit);

	if (map.Width() > 0 && map.Height() > 0)
	{
		cc::Vec2 clamped = cc::ToGlm(unit.position);
		clamped.x =
			std::clamp(clamped.x, 0.0f, static_cast<float>(map.Width()) * cc::TILE_SIZE - 1.0f);
		clamped.y =
			std::clamp(clamped.y, 0.0f, static_cast<float>(map.Height()) * cc::TILE_SIZE - 1.0f);
		unit.position = cc::ToRaylib(cc::SnapToTile(clamped));
	}
}

constexpr float kBlockedRetryDelaySeconds = 0.5f;
constexpr int kMaxBlockedRepaths = 3;

bool TryBlockedRetryWithBudget(Unit &unit, const OrderContext &ctx, float dtSeconds,
							   float &blockedTime, int &blockedRepaths)
{
	unit.velocity = { 0.0f, 0.0f };
	blockedTime += dtSeconds;
	if (blockedTime < kBlockedRetryDelaySeconds)
	{
		return true;
	}
	if (blockedRepaths >= kMaxBlockedRepaths)
	{
		return false;
	}
	++blockedRepaths;
	blockedTime = 0.0f;
	const cc::IVec2 start = cc::WorldToTile(cc::ToGlm(unit.position));
	const cc::IVec2 goal = cc::WorldToTile(cc::SnapToTile(cc::ToGlm(ctx.mover.moveTarget)));
	TilePath fresh;
	if (ctx.occ != nullptr)
	{
		fresh = FindPathFootprint(ctx.map, *ctx.occ, start, goal, unit.footprintWidth,
								  unit.footprintHeight, ctx.self, ctx.selfGen,
								  LocomotorOf(unit.type));
	}
	else
	{
		fresh = FindPath(ctx.map, start, goal, LocomotorOf(unit.type));
	}
	if (fresh.empty())
	{
		return true;
	}
	if (fresh.size() == 1)
	{
		Arrive(unit, ctx.mover, cc::ToGlm(ctx.mover.moveTarget));
		OnOrderFinished(unit, ctx);
		return true;
	}
	ctx.mover.path = std::move(fresh);
	ctx.mover.pathNext = 1;
	ctx.mover.hasPath = true;
	ctx.mover.hasMoveOrder = true;
	return true;
}

bool TryBlockedRetry(Unit &unit, const OrderContext &ctx, float dtSeconds)
{
	return TryBlockedRetryWithBudget(unit, ctx, dtSeconds, ctx.mover.blockedTime,
									 ctx.mover.blockedRepaths);
}

struct TileHash
{
	std::size_t operator()(cc::IVec2 tile) const
	{
		return (static_cast<std::size_t>(static_cast<std::uint32_t>(tile.x)) * 73856093u) ^
			   (static_cast<std::size_t>(static_cast<std::uint32_t>(tile.y)) * 19349663u);
	}
};

bool FreelyIdle(const Registry &registry, Entity entity)
{
	const Unit *unit = registry.Get<Unit>(entity);
	if (unit == nullptr || unit->health <= 0.0f || unit->state != UnitState::Idle ||
		IsEmbarked(registry, entity))
	{
		return false;
	}
	if (const Mover *mover = registry.Get<Mover>(entity))
	{
		if (mover->hasMoveOrder || mover->hasPath || mover->yielding)
		{
			return false;
		}
	}
	if (const Orders *orders = registry.Get<Orders>(entity))
	{
		if (orders->stance == Stance::Hold || orders->hasPatrol || orders->hasRepairOrder ||
			orders->hasLoadOrder || orders->hasUnloadOrder || orders->hasAttackGroundOrder ||
			!orders->orderQueue.empty())
		{
			return false;
		}
	}
	if (const CombatState *combat = registry.Get<CombatState>(entity))
	{
		if (combat->target != kInvalidEntity)
		{
			return false;
		}
	}
	return true;
}

bool OnTile(const OccupancyGrid &occ, cc::IVec2 tile, Entity entity)
{
	return occ.InBounds(tile) && occ.GetUnit(tile).entity == entity;
}

bool FindYieldTile(const TileMap &map, const OccupancyGrid &occ, const Mover &requester,
				   cc::IVec2 from, Entity self, std::uint32_t selfGen, int footprintW,
				   int footprintH, cc::IVec2 &out)
{
	const cc::IVec2 offsets[8] = { { 0, -1 }, { -1, 0 },  { 1, 0 },  { 0, 1 },
								   { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
	const cc::IVec2 requesterGoal = cc::WorldToTile(cc::ToGlm(requester.moveTarget));
	cc::IVec2 requesterTile{ -1, -1 };
	if (requester.pathNext > 0 && requester.pathNext <= requester.path.size())
	{
		requesterTile = requester.path[requester.pathNext - 1];
	}
	for (const cc::IVec2 &offset : offsets)
	{
		const cc::IVec2 candidate{ from.x + offset.x, from.y + offset.y };
		if (candidate == from || candidate == requesterGoal || candidate == requesterTile)
		{
			continue;
		}
		if (!occ.CanEnter(map, candidate, footprintW, footprintH, self, selfGen))
		{
			continue;
		}
		bool onPath = false;
		for (std::size_t i = requester.pathNext; i < requester.path.size(); ++i)
		{
			if (requester.path[i] == candidate)
			{
				onPath = true;
				break;
			}
		}
		if (onPath)
		{
			continue;
		}
		out = candidate;
		return true;
	}
	return false;
}

bool TryBeginYield(Unit &unit, Mover &mover, const OccupancyGrid *occ, cc::IVec2 blockedTile,
				   Entity self)
{
	if (occ == nullptr)
	{
		return false;
	}
	if (mover.awaitingClearBlocker != kInvalidEntity)
	{
		return true;
	}
	const OccEntry entry = occ->GetUnit(blockedTile);
	if (entry.entity == kOccEmpty || entry.entity == self || entry.entity == mover.yieldDeclined)
	{
		return false;
	}
	if (mover.yieldRequestBlocker == entry.entity && mover.yieldRequestTile == blockedTile)
	{
		return false;
	}
	mover.yieldRequestBlocker = entry.entity;
	mover.yieldRequestTile = blockedTile;
	mover.blockedTime = 0.0f;
	unit.velocity = { 0.0f, 0.0f };
	return true;
}

}

Facing FacingFromVelocity(Vector2 velocity)
{
	constexpr float kPi = 3.141592653589793f;
	float angle = std::atan2(velocity.y, velocity.x);
	if (angle < 0.0f)
	{
		angle += 2.0f * kPi;
	}
	const int octant =
		static_cast<int>((angle + kPi / 8.0f) / (kPi / 4.0f)) % 8;
	return static_cast<Facing>((6 - octant + 8) % 8);
}

void UpdateUnitMovement(Unit &unit, const OrderContext &ctx, CombatState &combat,
						float speedPixelsPerSec, float dtSeconds)
{
	if (!ctx.mover.hasMoveOrder && !ctx.mover.hasPath)
	{
		return;
	}

	const float step = speedPixelsPerSec * dtSeconds;
	unit.state = UnitState::Moving;
	combat.phase = AttackPhase::Ready;

	if (ctx.mover.hasPath)
	{
		if (ctx.mover.pathNext >= ctx.mover.path.size())
		{
			Arrive(unit, ctx.mover, cc::ToGlm(ctx.mover.moveTarget));
			OnOrderFinished(unit, ctx);
			return;
		}

		const cc::IVec2 node = ctx.mover.path[ctx.mover.pathNext];
		const cc::Vec2 waypoint = cc::TileToWorld(node.x, node.y);
		cc::Vec2 next = cc::ToGlm(unit.position);
		cc::IVec2 blockedTile{ -1, -1 };
		switch (StepToward(cc::ToGlm(unit.position), waypoint, step, ctx.map, next,
						   ctx.occ, ctx.self, ctx.selfGen, unit.footprintWidth,
						   unit.footprintHeight, &blockedTile))
		{
		case StepResult::Arrived:
			unit.position = cc::ToRaylib(waypoint);
			++ctx.mover.pathNext;
			if (ctx.mover.pathNext >= ctx.mover.path.size())
			{
				Arrive(unit, ctx.mover, cc::ToGlm(ctx.mover.moveTarget));
				OnOrderFinished(unit, ctx);
			}
			else
			{
				unit.velocity = { 0.0f, 0.0f };
			}
			return;
		case StepResult::Blocked:
			CancelAtBlocked(unit, ctx.mover, ctx.map);
			OnOrderCancelled(ctx.orders);
			return;
		case StepResult::BlockedUnit:
			if (TryBeginYield(unit, ctx.mover, ctx.occ, blockedTile, ctx.self))
			{
				return;
			}
			if (TryBlockedRetry(unit, ctx, dtSeconds))
			{
				return;
			}
			CancelAtBlocked(unit, ctx.mover, ctx.map);
			OnOrderCancelled(ctx.orders);
			return;
		case StepResult::Stepped:
			ctx.mover.blockedTime = 0.0f;
			unit.velocity = cc::ToRaylib((waypoint - cc::ToGlm(unit.position)) /
										 glm::length(waypoint - cc::ToGlm(unit.position)) * speedPixelsPerSec);
			unit.facing = FacingFromVelocity(unit.velocity);
			unit.position = cc::ToRaylib(next);
			return;
		}
	}

	cc::Vec2 next = cc::ToGlm(unit.position);
	cc::IVec2 blockedTile{ -1, -1 };
	switch (StepToward(cc::ToGlm(unit.position), cc::ToGlm(ctx.mover.moveTarget), step, ctx.map,
					   next, ctx.occ, ctx.self, ctx.selfGen, unit.footprintWidth,
					   unit.footprintHeight, &blockedTile))
	{
	case StepResult::Arrived:
		Arrive(unit, ctx.mover, cc::ToGlm(ctx.mover.moveTarget));
		OnOrderFinished(unit, ctx);
		return;
	case StepResult::Blocked:
		CancelAtBlocked(unit, ctx.mover, ctx.map);
		OnOrderCancelled(ctx.orders);
		return;
	case StepResult::BlockedUnit:
		if (TryBeginYield(unit, ctx.mover, ctx.occ, blockedTile, ctx.self))
		{
			return;
		}
		if (TryBlockedRetry(unit, ctx, dtSeconds))
		{
			return;
		}
		CancelAtBlocked(unit, ctx.mover, ctx.map);
		OnOrderCancelled(ctx.orders);
		return;
	case StepResult::Stepped:
		ctx.mover.blockedTime = 0.0f;
		unit.velocity = cc::ToRaylib((cc::ToGlm(ctx.mover.moveTarget) - cc::ToGlm(unit.position)) /
									 glm::length(cc::ToGlm(ctx.mover.moveTarget) - cc::ToGlm(unit.position)) *
									 speedPixelsPerSec);
		unit.facing = FacingFromVelocity(unit.velocity);
		unit.position = cc::ToRaylib(next);
		return;
	}
}

void PumpYieldHandshake(Registry &registry, const TileMap &map, OccupancyGrid &occ,
						float dtSeconds)
{
	constexpr float kYieldTimeoutSeconds = 3.0f;

	registry.Each<Mover>([&](Entity, Mover &mover) {
		if (mover.yielding && !mover.hasMoveOrder && !mover.hasPath)
		{
			mover.yielding = false;
		}
	});

	std::vector<Entity> requesters;
	registry.Each<Unit>([&](Entity id, const Unit &) {
		const Mover *mover = FindMover(registry, id);
		if (mover != nullptr && mover->yieldRequestBlocker != kInvalidEntity)
		{
			requesters.push_back(id);
		}
	});
	std::sort(requesters.begin(), requesters.end());

	std::vector<Entity> claimed;
	for (Entity id : requesters)
	{
		Unit *unit = registry.Get<Unit>(id);
		Mover *mover = registry.Get<Mover>(id);
		if (unit == nullptr || mover == nullptr)
		{
			continue;
		}
		const Entity blocker = mover->yieldRequestBlocker;
		const cc::IVec2 tile = mover->yieldRequestTile;
		mover->yieldRequestBlocker = kInvalidEntity;
		mover->yieldRequestTile = { -1, -1 };
		auto decline = [&] { mover->yieldDeclined = blocker; };
		if (unit->health <= 0.0f || IsEmbarked(registry, id) || blocker == kInvalidEntity)
		{
			continue;
		}
		if (std::find(claimed.begin(), claimed.end(), blocker) != claimed.end())
		{
			decline();
			continue;
		}
		Unit *aside = registry.Get<Unit>(blocker);
		Mover *asideMover = registry.Get<Mover>(blocker);
		if (aside == nullptr || asideMover == nullptr || aside->health <= 0.0f)
		{
			decline();
			continue;
		}
		if (aside->teamID != unit->teamID)
		{
			decline();
			continue;
		}
		if (!OnTile(occ, tile, blocker))
		{
			decline();
			continue;
		}
		if (!FreelyIdle(registry, blocker))
		{
			decline();
			continue;
		}
		cc::IVec2 side{ -1, -1 };
		if (!FindYieldTile(map, occ, *mover, tile, blocker, registry.Generation(blocker),
						   aside->footprintWidth, aside->footprintHeight, side))
		{
			decline();
			continue;
		}
		IssuePathOrderFootprint(*aside, MakeOrderContext(registry, blocker, map, &occ),
								cc::ToRaylib(cc::TileToWorld(side.x, side.y)));
		if (asideMover->path.empty() || !asideMover->hasMoveOrder)
		{
			asideMover->hasMoveOrder = false;
			asideMover->hasPath = false;
			asideMover->path.clear();
			asideMover->pathNext = 0;
			asideMover->moveTarget = {};
			decline();
			continue;
		}
		asideMover->yielding = true;
		claimed.push_back(blocker);
		mover->awaitingClearBlocker = blocker;
		mover->awaitingClearTile = tile;
		mover->awaitingClearTime = 0.0f;
		mover->blockedTime = 0.0f;
	}

	std::vector<Entity> waiting;
	registry.Each<Unit>([&](Entity id, const Unit &) {
		const Mover *mover = FindMover(registry, id);
		if (mover != nullptr && mover->awaitingClearBlocker != kInvalidEntity)
		{
			waiting.push_back(id);
		}
	});
	for (Entity id : waiting)
	{
		Mover *mover = registry.Get<Mover>(id);
		Unit *unit = registry.Get<Unit>(id);
		if (mover == nullptr || unit == nullptr)
		{
			continue;
		}
		const Entity blocker = mover->awaitingClearBlocker;
		Unit *aside = registry.Get<Unit>(blocker);
		Mover *asideMover = registry.Get<Mover>(blocker);
		mover->awaitingClearTime += dtSeconds;
		const bool bodyGone =
			aside == nullptr || aside->health <= 0.0f || !OnTile(occ, mover->awaitingClearTile, blocker);
		const bool asideIdle = aside != nullptr && aside->state == UnitState::Idle &&
							   (asideMover == nullptr ||
								(!asideMover->hasMoveOrder && !asideMover->hasPath));
		if (bodyGone && asideIdle)
		{
			mover->awaitingClearBlocker = kInvalidEntity;
			mover->awaitingClearTime = 0.0f;
			mover->blockedTime = 0.0f;
			mover->blockedRepaths = 0;
			continue;
		}
		if (mover->awaitingClearTime > kYieldTimeoutSeconds || (asideIdle && !bodyGone))
		{
			mover->awaitingClearBlocker = kInvalidEntity;
			mover->awaitingClearTime = 0.0f;
			mover->yieldDeclined = blocker;
		}
	}
}

void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ)
{
	std::unordered_map<cc::IVec2, std::vector<Entity>, TileHash> byTile;
	registry.Each<Unit>([&](Entity id, Unit &unit) {
		if (unit.health > 0.0f && !IsEmbarked(registry, id))
		{
			byTile[cc::WorldToTile(cc::ToGlm(unit.position))].push_back(id);
		}
	});

	std::vector<cc::IVec2> claimedThisFrame;

	for (auto &[tile, occupants] : byTile)
	{
		if (occupants.size() < 2)
		{
			continue;
		}
		if (!occ.InBounds(tile))
		{
			continue;
		}
		const OccEntry holder = occ.GetUnit(tile);
		bool allIdle = true;
		bool anyDeadlockedNow = false;
		Entity idleNonHolderPick = kInvalidEntity;
		Entity idleFallbackPick = kInvalidEntity;
		Entity deadlockedNonHolderPick = kInvalidEntity;
		Entity anyNonHolderPick = kInvalidEntity;
		for (Entity id : occupants)
		{
			const Unit *unit = registry.Get<Unit>(id);
			if (unit == nullptr)
			{
				continue;
			}
			const bool isHolder =
				holder.entity == id && holder.generation == registry.Generation(id);
			const Mover *mover = FindMover(registry, id);
			const bool hasOrder =
				mover != nullptr && (mover->hasMoveOrder || mover->hasPath);
			const bool isDeadlockedNow =
				hasOrder && mover->blockedRepaths >= kMaxBlockedRepaths && mover->blockedTime > 0.0f;
			if (hasOrder)
			{
				allIdle = false;
				if (isDeadlockedNow)
				{
					anyDeadlockedNow = true;
				}
			}
			if (!isHolder && (anyNonHolderPick == kInvalidEntity || id < anyNonHolderPick))
			{
				anyNonHolderPick = id;
			}
			if (!hasOrder)
			{
				if (idleFallbackPick == kInvalidEntity || id < idleFallbackPick)
				{
					idleFallbackPick = id;
				}
				if (!isHolder && (idleNonHolderPick == kInvalidEntity || id < idleNonHolderPick))
				{
					idleNonHolderPick = id;
				}
			}
			if (isDeadlockedNow && !isHolder &&
				(deadlockedNonHolderPick == kInvalidEntity || id < deadlockedNonHolderPick))
			{
				deadlockedNonHolderPick = id;
			}
		}
		Entity pick = kInvalidEntity;
		if (allIdle)
		{
			pick = (idleNonHolderPick != kInvalidEntity) ? idleNonHolderPick : idleFallbackPick;
		}
		else if (anyDeadlockedNow)
		{
			pick = (deadlockedNonHolderPick != kInvalidEntity) ? deadlockedNonHolderPick
															   : anyNonHolderPick;
		}
		if (pick == kInvalidEntity)
		{
			continue;
		}

		Unit *unit = registry.Get<Unit>(pick);
		cc::IVec2 dest = NearestEnterableTile(map, occ, tile, unit->footprintWidth,
											  unit->footprintHeight, pick,
											  registry.Generation(pick));
		for (int guard = 0; guard < 8 &&
							std::find(claimedThisFrame.begin(), claimedThisFrame.end(), dest) !=
								claimedThisFrame.end();
			 ++guard)
		{
			dest = NearestEnterableTile(map, occ, dest + cc::IVec2(1, 0), unit->footprintWidth,
										unit->footprintHeight, pick,
										registry.Generation(pick));
		}
		claimedThisFrame.push_back(dest);

		IssuePathOrderFootprint(*unit, MakeOrderContext(registry, pick, map, &occ),
								cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)));
		// The stacked start tile blocks the footprint pathfinder (the neighbor
		// still owns it), so fall back to a straight move order: step-out walks
		// the unit clear and repaths from the freed tile.
		if (FindMover(registry, pick)->path.empty())
		{
			IssueMoveOrder(*unit, GetOrders(registry, pick), GetMover(registry, pick),
						   cc::ToRaylib(cc::TileToWorld(dest.x, dest.y)));
		}
	}
}
