// Stacked-order deadlock guards: staggered formation moves from a shared
// start tile, and ResolveStackedUnits rescue near the retry ceiling.

#include "test_harness.h"

#include "units/Extensions.h"
#include "units/Formation.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

namespace
{
Unit MakeInfantry(int tileX, int tileY)
{
	Unit unit;
	unit.type = UnitType::RifleInfantry;
	ApplyBaseStats(unit);
	unit.teamID = kPlayerTeam;
	unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	return unit;
}

// Runs the pipeline until done, tracking the longest zero-velocity streak.
struct RunResult
{
	int completedAtFrame = -1;
	int maxConsecutiveStillFrames = 0;
};

RunResult RunUntilDone(Registry &registry, TileMap &map, OccupancyGrid &occ,
					   const std::vector<Entity> &squad, int budget)
{
	RunResult result;
	std::vector<int> stillStreak(squad.size(), 0);
	constexpr float kDt = 1.0f / 60.0f;
	for (int frame = 0; frame < budget; ++frame)
	{
		RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
		bool allDone = true;
		for (std::size_t i = 0; i < squad.size(); ++i)
		{
			const Unit *u = registry.Get<Unit>(squad[i]);
			const Mover *mover = registry.Get<Mover>(squad[i]);
			allDone = allDone && mover != nullptr && !mover->hasMoveOrder && !mover->hasPath;
			const bool active = mover != nullptr && (mover->hasMoveOrder || mover->hasPath);
			const bool still = u->velocity.x == 0.0f && u->velocity.y == 0.0f;
			if (active && still)
			{
				++stillStreak[i];
				if (stillStreak[i] > result.maxConsecutiveStillFrames)
				{
					result.maxConsecutiveStillFrames = stillStreak[i];
				}
			}
			else
			{
				stillStreak[i] = 0;
			}
		}
		if (allDone)
		{
			result.completedAtFrame = frame;
			break;
		}
	}
	return result;
}
} // namespace

void RunFormationDeadlockFixTests()
{
	// Two co-located units issued a formation move must never sit still long
	// with an active order, and both must reach their original slots.
	{
		TileMap map(30, 30);
		OccupancyGrid occ(30, 30);
		Registry registry;
		const Entity a = registry.Create();
		registry.Add(a, MakeInfantry(10, 10));
		const Entity b = registry.Create();
		registry.Add(b, MakeInfantry(10, 10));
		const std::vector<Entity> squad{ a, b };

		formation::IssueFormationMoveFP(registry, squad, map, occ,
										cc::ToRaylib(cc::TileToWorld(20, 10)), false);
		// One direct (the keeper), one via the escape hop (the straggler).
		for (Entity id : squad)
		{
			const Mover *mover = registry.Get<Mover>(id);
			CC_CHECK(mover != nullptr && (mover->hasMoveOrder || mover->hasPath));
		}
		// cellSize-1 slots for 2 units are (20,10) and (21,10).
		const cc::IVec2 slotA{ 20, 10 };
		const cc::IVec2 slotB{ 21, 10 };

		const RunResult result = RunUntilDone(registry, map, occ, squad, 2400);
		CC_CHECK(result.completedAtFrame >= 0); // never freezes forever
		// No unit may sit motionless with an active order beyond a handful of
		// frames; ~90+ frames is the silent-cancel stall.
		CC_CHECK(result.maxConsecutiveStillFrames < 10);

		const cc::IVec2 tileA = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(a)->position));
		const cc::IVec2 tileB = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(b)->position));
		// Destinations come from the bottleneck slot assignment: on tied costs
		// the lower entity takes the lower slot, so a==slotA and b==slotB.
		CC_CHECK(tileA == slotA);
		CC_CHECK(tileB == slotB);
	}

	{
		TileMap map(30, 30);
		OccupancyGrid occ(30, 30);
		Registry registry;
		std::vector<Entity> squad;
		for (int i = 0; i < 4; ++i)
		{
			const Entity e = registry.Create();
			registry.Add(e, MakeInfantry(5, 5));
			squad.push_back(e);
		}

		formation::IssueFormationMoveFP(registry, squad, map, occ,
										cc::ToRaylib(cc::TileToWorld(20, 20)), false);
		for (Entity id : squad)
		{
			const Mover *mover = registry.Get<Mover>(id);
			CC_CHECK(mover != nullptr && (mover->hasMoveOrder || mover->hasPath));
		}

		const RunResult result = RunUntilDone(registry, map, occ, squad, 2400);
		CC_CHECK(result.completedAtFrame >= 0);
		CC_CHECK(result.maxConsecutiveStillFrames < 10);

		std::vector<cc::IVec2> finalTiles;
		for (Entity id : squad)
		{
			const Unit *u = registry.Get<Unit>(id);
			finalTiles.push_back(cc::WorldToTile(cc::ToGlm(u->position)));
		}
		for (std::size_t i = 0; i < finalTiles.size(); ++i)
		{
			for (std::size_t j = i + 1; j < finalTiles.size(); ++j)
			{
				CC_CHECK(!(finalTiles[i] == finalTiles[j]));
			}
		}
	}

	// ResolveStackedUnits rescues a unit one blocked frame from a silent
	// cancel while it shares a tile with another ordered unit.
	{
		TileMap map(20, 20);
		OccupancyGrid occ(20, 20);
		Registry registry;
		const Entity a = registry.Create();
		registry.Add(a, MakeInfantry(8, 8));
		const Entity b = registry.Create();
		registry.Add(b, MakeInfantry(8, 8));

		Unit *ua = registry.Get<Unit>(a);
		Unit *ub = registry.Get<Unit>(b);
		(void)ua;
		(void)ub;
		Mover &ma = GetMover(registry, a);
		Mover &mb = GetMover(registry, b);
		// Both hold an order but neither moved; A's retry budget is maxed, so
		// one more blocked frame would silently cancel it.
		ma.hasMoveOrder = true;
		ma.moveTarget = cc::ToRaylib(cc::TileToWorld(15, 8));
		ma.blockedRepaths = 3; // matches Unit.cpp's private kMaxBlockedRepaths
		ma.blockedTime = 0.1f; // currently mid-block, not just a past history of repaths
		mb.hasMoveOrder = true;
		mb.moveTarget = cc::ToRaylib(cc::TileToWorld(8, 15));
		mb.blockedRepaths = 0; // B's order is still healthy

		// Seed occupancy the way RunUnitMovementFrame's pre-pass would.
		registry.Each<Unit>([&](Entity id, Unit &unit) {
			const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
			occ.ReleaseFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
									  registry.Generation(id));
			(void)occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
											registry.Generation(id));
		});
		// The first-come reservation holder is never picked (its own tile
		// looks empty to it), so assert whichever branch this run landed in.
		const OccEntry holder = occ.GetUnit(cc::WorldToTile(cc::ToGlm(ua->position)));
		const bool aIsHolder = holder.entity == a && holder.generation == registry.Generation(a);

		ResolveStackedUnits(registry, map, occ);

		if (aIsHolder)
		{
			// A is the holder: its order is left as-is and B is relocated to
			// free the tile for A's next attempt.
			CC_CHECK(ma.moveTarget.x == cc::ToRaylib(cc::TileToWorld(15, 8)).x &&
					 ma.moveTarget.y == cc::ToRaylib(cc::TileToWorld(15, 8)).y);
			CC_CHECK(mb.hasPath);
			const cc::IVec2 rescueDest = cc::WorldToTile(cc::ToGlm(mb.moveTarget));
			CC_CHECK(!(rescueDest == cc::IVec2(8, 8)));
		}
		else
		{
			// A deadlocked non-holder is rescued directly with a fresh,
			// different destination; B's healthy order stays untouched.
			CC_CHECK(ma.hasPath);
			const cc::IVec2 rescueDest = cc::WorldToTile(cc::ToGlm(ma.moveTarget));
			CC_CHECK(!(rescueDest == cc::IVec2(8, 8)));
			CC_CHECK(mb.moveTarget.x == cc::ToRaylib(cc::TileToWorld(8, 15)).x &&
					 mb.moveTarget.y == cc::ToRaylib(cc::TileToWorld(8, 15)).y);
			CC_CHECK(mb.blockedRepaths == 0);
		}
	}
}
