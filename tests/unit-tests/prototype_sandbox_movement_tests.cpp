// Sandbox squad pathing against the real prototype.map (water blob, tree
// cluster): every order resolves and arrives, never wedges on a shared tile.

#include "test_harness.h"

#include "units/AICommander.h"
#include "economy/Building.h"
#include "units/Combat.h"
#include "core/Event.h"
#include "units/Extensions.h"
#include "world/FogOfWar.h"
#include "app/ui/GameCamera.h"
#include "world/MapFile.h"
#include "economy/Nodes.h"
#include "world/Pathfinder.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/match/Skirmish.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

#include <vector>

namespace
{

std::string ShippedMap(const std::string &name)
{
	const std::string candidates[] = { "data/maps/" + name, "../../data/maps/" + name,
									   "../../../data/maps/" + name };
	for (const std::string &path : candidates)
	{
		MapData probe;
		if (ParseMapFile(path, probe))
		{
			return path;
		}
	}
	return "";
}

struct Harness
{
	Registry registry;
	ResourceSystem resources;
	TileMap map{ 20, 15 };
	OccupancyGrid occ{ 20, 15 };
	FogOfWar fog;
	ResourceNodes nodes;
	ProductionQueue bootcampQueue{ ProductionCategory::Infantry };
	ProductionQueue workshopQueue{ ProductionCategory::Vehicle };
	EventDispatcher events;
	UnitFactory factory{ registry, resources, events };
	GameCamera camera;
	AICommander ai{ registry, map, nodes, events, kEnemyTeam, AIDifficulty::Medium, { 0, 0 },
					{ 0, 0 } };
	SkirmishWorld world{ &registry, &resources, &map, &occ, &fog, &nodes,
						 &bootcampQueue, &workshopQueue, &factory, &ai, nullptr, nullptr,
						 &camera };
};

} // namespace

void RunPrototypeSandboxMovementTests()
{
	const std::string proto = ShippedMap("prototype.map");
	CC_CHECK(!proto.empty());
	if (proto.empty())
	{
		return;
	}

	Harness sand;
	CC_CHECK(BuildSandbox(sand.world, proto));
	CC_CHECK(sand.map.Width() == 20 && sand.map.Height() == 10);

	std::vector<Entity> squad;
	sand.registry.Each<Unit>([&](Entity id, const Unit &unit) {
		if (unit.teamID == kPlayerTeam && !IsVehicleHull(unit.type))
		{
			squad.push_back(id);
		}
	});
	CC_CHECK(!squad.empty());
	if (squad.empty())
	{
		return;
	}
	if (squad.size() > 6)
	{
		squad.resize(6);
	}

	const cc::IVec2 destinations[] = {
		{ 1, 0 },   { 18, 0 },  { 1, 8 }, { 17, 4 }, { 5, 8 }, { 10, 5 },
	};
	for (const cc::IVec2 &dest : destinations)
	{
		CC_CHECK(!sand.map.IsBlocked(dest));
	}

	for (std::size_t i = 0; i < squad.size(); ++i)
	{
		Unit *u = sand.registry.Get<Unit>(squad[i]);
		IssuePathOrderFootprint(*u, MakeOrderContext(sand.registry, squad[i], sand.map, &sand.occ),
								cc::ToRaylib(cc::TileToWorld(
									destinations[i].x, destinations[i].y)));
		CC_CHECK(sand.registry.Get<Mover>(squad[i])->hasPath ||
				 sand.registry.Get<Mover>(squad[i])->hasMoveOrder); // path found or straight-line
	}

	constexpr float kDt = 1.0f / 60.0f;
	constexpr int kBudget = 2400; // 40s at 60fps
	std::vector<bool> done(squad.size(), false);
	std::vector<int> doneFrame(squad.size(), -1);
	int frame = 0;
	for (; frame < kBudget; ++frame)
	{
		RunUnitMovementFrame(sand.registry, sand.map, sand.occ, &sand.fog, kDt);
		bool allDone = true;
		for (std::size_t i = 0; i < squad.size(); ++i)
		{
			const Mover *mover = sand.registry.Get<Mover>(squad[i]);
			if (!done[i] && mover != nullptr && !mover->hasMoveOrder && !mover->hasPath)
			{
				done[i] = true;
				doneFrame[i] = frame;
			}
			allDone = allDone && done[i];
		}
		if (allDone)
		{
			break;
		}
	}

	for (std::size_t i = 0; i < squad.size(); ++i)
	{
		CC_CHECK(done[i]);
	}
	CC_CHECK(frame < kBudget);

	// Destinations are reachable with no dead end: a short cancel here means
	// stacked-unit resolution regressed.
	for (std::size_t i = 0; i < squad.size(); ++i)
	{
		const Unit *u = sand.registry.Get<Unit>(squad[i]);
		CC_CHECK(cc::WorldToTile(cc::ToGlm(u->position)) == destinations[i]);
	}

	for (std::size_t i = 0; i < squad.size(); ++i)
	{
		for (std::size_t j = i + 1; j < squad.size(); ++j)
		{
			const Unit *ui = sand.registry.Get<Unit>(squad[i]);
			const Unit *uj = sand.registry.Get<Unit>(squad[j]);
			CC_CHECK(!(cc::WorldToTile(cc::ToGlm(ui->position)) ==
					  cc::WorldToTile(cc::ToGlm(uj->position))));
		}
	}
}
