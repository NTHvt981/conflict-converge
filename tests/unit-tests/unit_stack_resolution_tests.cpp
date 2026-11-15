// ResolveStackedUnits relocates one unit per shared anchor tile via an ordinary
// footprint-aware move order; stacked squads must never deadlock or wedge.

#include "test_harness.h"

#include "units/Extensions.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"

namespace
{

Unit MakeUnit(int tileX, int tileY, UnitType type = UnitType::RifleInfantry)
{
	Unit unit;
	unit.type = type;
	ApplyBaseStats(unit); // sets footprintWidth/Height (2x2 for vehicles)
	unit.position = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	return unit;
}

// ResolveStackedUnits reads OccupancyGrid but never fills it; direct callers
// must reproduce RunUnitMovementFrame's occupancy pre-pass.
void SeedOccupancy(Registry &registry, OccupancyGrid &occ)
{
	registry.Each<Unit>([&](Entity id, Unit &unit) {
		if (unit.health > 0.0f)
		{
			const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(unit.position));
			occ.ReleaseFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
									  registry.Generation(id));
			(void)occ.ReserveFootprintOwned(anchor, unit.footprintWidth, unit.footprintHeight, id,
											registry.Generation(id));
		}
	});
}

} // namespace

void RunUnitStackResolutionTests()
{
	{
		TileMap map(10, 10);
		OccupancyGrid occ(10, 10);
		Registry registry;
		const Entity a = registry.Create();
		registry.Add(a, MakeUnit(5, 5));
		const Entity b = registry.Create();
		registry.Add(b, MakeUnit(5, 5));

		SeedOccupancy(registry, occ);
		ResolveStackedUnits(registry, map, occ);

		const Mover *ua = registry.Get<Mover>(a);
		const Mover *ub = registry.Get<Mover>(b);
		const bool aActive = ua != nullptr && (ua->hasMoveOrder || ua->hasPath);
		const bool bActive = ub != nullptr && (ub->hasMoveOrder || ub->hasPath);
		CC_CHECK(aActive != bActive); // exactly one, not both, not neither

		SeedOccupancy(registry, occ);
		ResolveStackedUnits(registry, map, occ);
		const Mover *ua2 = registry.Get<Mover>(a);
		const Mover *ub2 = registry.Get<Mover>(b);
		const bool aActive2 = ua2 != nullptr && (ua2->hasMoveOrder || ua2->hasPath);
		const bool bActive2 = ub2 != nullptr && (ub2->hasMoveOrder || ub2->hasPath);
		CC_CHECK(aActive2 != bActive2);
		CC_CHECK(aActive == aActive2 && bActive == bActive2); // same one stays picked
	}

	{
		TileMap map(10, 10);
		OccupancyGrid occ(10, 10);
		Registry registry;
		const Entity a = registry.Create();
		registry.Add(a, MakeUnit(5, 5));
		const Entity b = registry.Create();
		registry.Add(b, MakeUnit(5, 5));

		constexpr float kDt = 1.0f / 60.0f;
		int frame = 0;
		for (; frame < 300; ++frame)
		{
			RunUnitMovementFrame(registry, map, occ, nullptr, kDt);
			const Mover *ua = registry.Get<Mover>(a);
			const Mover *ub = registry.Get<Mover>(b);
			if (ua != nullptr && ub != nullptr && !ua->hasMoveOrder && !ua->hasPath &&
				!ub->hasMoveOrder && !ub->hasPath)
			{
				break;
			}
		}
		CC_CHECK(frame < 300);
		const cc::IVec2 tileA = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(a)->position));
		const cc::IVec2 tileB = cc::WorldToTile(cc::ToGlm(registry.Get<Unit>(b)->position));
		CC_CHECK(!(tileA == tileB));
	}

	{
		TileMap map(10, 10);
		OccupancyGrid occ(10, 10);
		Registry registry;
		const Entity a = registry.Create();
		registry.Add(a, MakeUnit(5, 5, UnitType::HeavyTank));
		const Entity b = registry.Create();
		registry.Add(b, MakeUnit(5, 5, UnitType::HeavyTank));
		CC_CHECK(registry.Get<Unit>(a)->footprintWidth == 2 &&
				registry.Get<Unit>(a)->footprintHeight == 2);

		SeedOccupancy(registry, occ);
		ResolveStackedUnits(registry, map, occ);

		const Mover *ua = registry.Get<Mover>(a);
		const Mover *ub = registry.Get<Mover>(b);
		const Entity picked = (ua != nullptr && (ua->hasMoveOrder || ua->hasPath)) ? a : b;
		const Mover *pickedMover = registry.Get<Mover>(picked);
		CC_CHECK(pickedMover != nullptr &&
				 (pickedMover->hasMoveOrder || pickedMover->hasPath));
		const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(pickedMover->moveTarget));
		CC_CHECK(!(dest == cc::IVec2(5, 5))); // actually moved off the stack tile
		CC_CHECK(occ.CanEnter(map, dest, 2, 2, picked, registry.Generation(picked)));
	}

	{
		TileMap map(20, 20);
		OccupancyGrid occ(20, 20);
		Registry registry;
		const Entity a0 = registry.Create();
		registry.Add(a0, MakeUnit(2, 2));
		const Entity a1 = registry.Create();
		registry.Add(a1, MakeUnit(2, 2));
		const Entity b0 = registry.Create();
		registry.Add(b0, MakeUnit(17, 17));
		const Entity b1 = registry.Create();
		registry.Add(b1, MakeUnit(17, 17));

		ResolveStackedUnits(registry, map, occ);

		auto stackProgressed = [&](Entity x, Entity y) {
			const Mover *ux = registry.Get<Mover>(x);
			const Mover *uy = registry.Get<Mover>(y);
			const bool xActive = ux != nullptr && (ux->hasMoveOrder || ux->hasPath);
			const bool yActive = uy != nullptr && (uy->hasMoveOrder || uy->hasPath);
			return xActive != yActive;
		};
		CC_CHECK(stackProgressed(a0, a1)); // near (2,2) resolved
		CC_CHECK(stackProgressed(b0, b1)); // near (17,17) resolved, independently
	}

	{
		TileMap map(10, 10);
		OccupancyGrid occ(10, 10);
		Registry registry;
		const Entity solo = registry.Create();
		registry.Add(solo, MakeUnit(1, 1));
		Unit *unit = registry.Get<Unit>(solo);
		IssueMoveOrder(*unit, GetOrders(registry, solo), GetMover(registry, solo),
					   cc::ToRaylib(cc::TileToWorld(8, 8)));
		const Vector2 targetBefore = GetMover(registry, solo).moveTarget;
		const bool hasMoveBefore = GetMover(registry, solo).hasMoveOrder;

		ResolveStackedUnits(registry, map, occ);

		const Mover *after = registry.Get<Mover>(solo);
		CC_CHECK(after != nullptr && after->hasMoveOrder == hasMoveBefore);
		CC_CHECK(after->moveTarget.x == targetBefore.x && after->moveTarget.y == targetBefore.y);
	}

	{
		TileMap map(10, 10);
		OccupancyGrid occ(10, 10);
		Registry registry;
		const Entity a = registry.Create();
		registry.Add(a, MakeUnit(5, 5));
		const Entity b = registry.Create();
		registry.Add(b, MakeUnit(5, 5));
		// Past the east edge: tile.x = 10 is OOB on a 10-wide map.
		registry.Get<Unit>(a)->position = Vector2{ 10.0f * 128.0f + 1.0f, 5.0f * 128.0f };
		registry.Get<Unit>(b)->position = Vector2{ 10.0f * 128.0f + 1.0f, 5.0f * 128.0f };

		SeedOccupancy(registry, occ);
		ResolveStackedUnits(registry, map, occ);

		// OOB stacks are skipped, not relocated.
		CC_CHECK(!GetMover(registry, a).hasMoveOrder && !GetMover(registry, a).hasPath);
		CC_CHECK(!GetMover(registry, b).hasMoveOrder && !GetMover(registry, b).hasPath);
	}

	// An off-map target within one step cancels in bounds instead of
	// teleporting the unit out (StepToward's arrive shortcut).
	{
		TileMap map(10, 10);
		OccupancyGrid occ(10, 10);
		Registry registry;
		const Entity solo = registry.Create();
		registry.Add(solo, MakeUnit(9, 5));
		Unit *unit = registry.Get<Unit>(solo);
		unit->speed = 200.0f;
		IssueMoveOrder(*unit, GetOrders(registry, solo), GetMover(registry, solo),
					   Vector2{ 10.0f * 128.0f + cc::kHalfTile, 5.0f * 128.0f }); // OOB target
		UpdateUnitMovement(*unit, MakeOrderContext(registry, solo, map, &occ),
						   GetCombatState(registry, solo),
						   EffectiveSpeed(*unit, GetMover(registry, solo)), 1.0f);
		const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(unit->position));
		CC_CHECK(map.InBounds(tile));
		CC_CHECK(!GetMover(registry, solo).hasMoveOrder && !GetMover(registry, solo).hasPath);
	}
}
