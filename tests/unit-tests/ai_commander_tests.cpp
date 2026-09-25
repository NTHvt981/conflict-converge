// Unit tests for the AICommander (params, setup, waves, scouting,
// headless skirmish). The commander drives the same per-frame calls as
// main.cpp; the skirmish below mirrors tests/integration-tests battle setup.

#include "test_harness.h"

#include "AICommander.h"
#include "Building.h" // building-count query
#include "Extensions.h"
#include "Nodes.h"
#include "TileMap.h"
#include "UnitFactory.h"

namespace
{

int CountBuildings(const Registry &registry, int teamID)
{
    int count = 0;
    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID == teamID && building.state == BuildingState::Operational)
        {
            ++count;
        }
    });
    return count;
}

void SweepDead(Registry &registry, UnitFactory &factory)
{
    std::vector<Entity> dead;
    registry.Each<Unit>([&](Entity id, const Unit &unit) {
        if (unit.health <= 0.0f)
        {
            dead.push_back(id);
        }
    });
    for (Entity id : dead)
    {
        factory.DestroyUnit(id);
    }
}

} // namespace

void RunAICommanderTests()
{
    // --- difficulty params scale monotonically (Easy/Medium/Hard) ---
    const AIDifficultyParams easy = ParamsFor(AIDifficulty::Easy);
    const AIDifficultyParams medium = ParamsFor(AIDifficulty::Medium);
    const AIDifficultyParams hard = ParamsFor(AIDifficulty::Hard);
    CC_CHECK(easy.harvesters <= medium.harvesters);
    CC_CHECK(medium.harvesters <= hard.harvesters);
    CC_CHECK(easy.scoutInterval > medium.scoutInterval);
    CC_CHECK(medium.scoutInterval > hard.scoutInterval);
    CC_CHECK(easy.waveThreshold > 0 && medium.waveThreshold > 0 && hard.waveThreshold > 0);
    CC_CHECK(easy.composition.size() == 1);
    CC_CHECK(hard.composition.size() > easy.composition.size());
    CC_CHECK(!easy.retreats && !medium.retreats && hard.retreats);

    // --- SetupBase: buildings placed, starting guard fielded ---
    Registry registry;
    TileMap map(20, 15);
    ResourceNodes nodes;
    EventDispatcher events;
    AICommander ai(registry, map, nodes, events, 1, AIDifficulty::Medium, { 16, 9 }, { 1, 10 });
    CC_CHECK(ai.TeamID() == 1);
    CC_CHECK(ai.Difficulty() == AIDifficulty::Medium);
    CC_CHECK(ai.WavesLaunched() == 0);
    CC_CHECK(!ai.HasScouted());
    ai.SetupBase();
    UpdateBuildingConstruction(registry, 20.0f); // sites -> Operational (game ticks this)
    CC_CHECK(CountBuildings(registry, 1) == 3);
    CC_CHECK(ai.CombatUnitCount() >= 1); // guard: team never starts empty

    // --- harvesters sustain to the difficulty count ---
    nodes.SpawnNode(map, ResourceKind::Iron, { 15, 3 }, 200.0f, 10.0f);
    for (int i = 0; i < 60; ++i)
    {
        ai.Update(1.0f / 60.0f);
    }
    CC_CHECK(ai.HarvesterCount() == medium.harvesters);

    // --- wave launches on army-size threshold, ordering the army ---
    Registry warRegistry;
    TileMap warMap(20, 15);
    ResourceNodes warNodes;
    EventDispatcher warEvents;
    ResourceSystem warResources;
    AICommander war(warRegistry, warMap, warNodes, warEvents, 1, AIDifficulty::Easy, { 16, 9 },
                    { 1, 10 });
    war.SetupBase();
    UnitFactory warFactory(warRegistry, warResources, warEvents);
    for (int i = 0; i < 3; ++i)
    {
        warFactory.SpawnPrepaid(UnitType::RifleInfantry, 1,
                                cc::ToRaylib(cc::TileToWorld(16 + i, 9)));
    }
    war.Update(1.0f / 60.0f);
    CC_CHECK(war.WavesLaunched() == 1);
    int ordered = 0;
    warRegistry.Each<Unit>([&](Entity id, const Unit &unit) {
        const Mover *mover = FindMover(warRegistry, id);
        if (unit.teamID == 1 && unit.health > 0.0f && mover != nullptr &&
            (mover->hasMoveOrder || mover->hasPath))
        {
            ++ordered;
        }
    });
    CC_CHECK(ordered >= 3);

    // --- scouting: Hard cadence dispatches within its interval ---
    Registry scoutRegistry;
    TileMap scoutMap(20, 15);
    ResourceNodes scoutNodes;
    EventDispatcher scoutEvents;
    AICommander scout(scoutRegistry, scoutMap, scoutNodes, scoutEvents, 1, AIDifficulty::Hard,
                      { 16, 9 }, { 1, 10 });
    scout.SetupBase();
    for (int i = 0; i < 16 * 60; ++i)
    {
        scout.Update(1.0f / 60.0f);
        UpdateBuildingConstruction(scoutRegistry, 1.0f / 60.0f);
    }
    CC_CHECK(scout.HasScouted());
    CC_CHECK(scout.LastSeenEnemy() == cc::IVec2(1, 10));

    // --- headless skirmish: two Easy commanders, economies grow, wave fires ---
    Registry simRegistry;
    TileMap simMap(24, 18);
    ResourceNodes simNodes;
    simNodes.SpawnNode(simMap, ResourceKind::Iron, { 3, 14 }, 300.0f, 10.0f);
    simNodes.SpawnNode(simMap, ResourceKind::Iron, { 20, 3 }, 300.0f, 10.0f);
    simNodes.SpawnNode(simMap, ResourceKind::Oil, { 20, 14 }, 200.0f, 10.0f);
    EventDispatcher simEvents;
    ResourceSystem simResources;
    UnitFactory simFactory(simRegistry, simResources, simEvents);
    AICommander left(simRegistry, simMap, simNodes, simEvents, 0, AIDifficulty::Easy, { 1, 14 },
                     { 21, 3 });
    AICommander right(simRegistry, simMap, simNodes, simEvents, 1, AIDifficulty::Easy, { 21, 3 },
                      { 1, 14 });
    left.SetupBase();
    right.SetupBase();
    const std::size_t startEntities = simRegistry.EntityCount();
    constexpr float kDt = 1.0f / 60.0f;
    for (int frame = 0; frame < 1500; ++frame)
    {
        left.Update(kDt);
        right.Update(kDt);
        UpdateBuildingConstruction(simRegistry, kDt); // sites -> Operational, like Game
        simNodes.Update(kDt);
        simNodes.GatherTick(simRegistry, simResources, kDt);
        simRegistry.Each<Unit>([&](Entity id, Unit &unit) {
            UpdateUnit(id, simRegistry, simMap, kDt);
        });
        SweepDead(simRegistry, simFactory);
    }
    CC_CHECK(left.WavesLaunched() + right.WavesLaunched() >= 1);
    CC_CHECK(simRegistry.EntityCount() > startEntities);
    CC_CHECK(left.HarvesterCount() + right.HarvesterCount() >= 1);

    // --- bound occupancy: harvester order sanitizes off the blocked node ---
    {
        Registry registry;
        TileMap map(20, 15);
        ResourceNodes nodes;
        EventDispatcher events;
        OccupancyGrid occ(20, 15);
        AICommander ai(registry, map, nodes, events, 1, AIDifficulty::Medium, { 16, 9 },
                       { 1, 10 });
        ai.SetupBase();
        ai.SetOccupancy(&occ);
        nodes.SpawnNode(map, ResourceKind::Iron, { 15, 3 }, 200.0f, 10.0f);
        const Entity blocker = registry.Create();
        occ.ReserveFootprint({ 15, 3 }, 1, 1, blocker, registry.Generation(blocker));
        for (int i = 0; i < 60; ++i)
        {
            ai.Update(1.0f / 60.0f);
        }
        CC_CHECK(ai.HarvesterCount() == medium.harvesters);
        registry.Each<Unit>([&](Entity id, const Unit &unit) {
            if (id == blocker || unit.type != UnitType::Engineer)
            {
                return;
            }
            const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(FindMover(registry, id)->moveTarget));
            CC_CHECK(!(dest == cc::IVec2(15, 3))); // beside the node, not inside
            CC_CHECK(FindMover(registry, id)->hasMoveOrder || FindMover(registry, id)->hasPath);
        });
    }

    // --- bound occupancy: wave keeps attack-move, slots avoid blockers ---
    {
        Registry registry;
        TileMap map(20, 15);
        ResourceNodes nodes;
        EventDispatcher events;
        OccupancyGrid occ(20, 15);
        ResourceSystem resources;
        AICommander ai(registry, map, nodes, events, 1, AIDifficulty::Easy, { 16, 9 },
                       { 1, 10 });
        ai.SetupBase();
        ai.SetOccupancy(&occ);
        UnitFactory factory(registry, resources, events);
        for (int i = 0; i < 3; ++i)
        {
            factory.SpawnPrepaid(UnitType::RifleInfantry, 1,
                                 cc::ToRaylib(cc::TileToWorld(16 + i, 9)));
        }
        // Wave slots around lastSeenEnemy (1,10): (1,10),(2,10),(1,11),(2,11).
        const Entity blocker = registry.Create();
        const std::uint32_t gen = registry.Generation(blocker);
        occ.ReserveFootprint({ 1, 10 }, 1, 1, blocker, gen);
        occ.ReserveFootprint({ 2, 10 }, 1, 1, blocker, gen);
        occ.ReserveFootprint({ 1, 11 }, 1, 1, blocker, gen);
        occ.ReserveFootprint({ 2, 11 }, 1, 1, blocker, gen);
        ai.Update(1.0f / 60.0f);
        CC_CHECK(ai.WavesLaunched() == 1);
        int ordered = 0;
        registry.Each<Unit>([&](Entity id, const Unit &unit) {
            if (id == blocker || unit.teamID != 1 || unit.health <= 0.0f)
            {
                return;
            }
            if (unit.type == UnitType::Engineer)
            {
                return;
            }
            const Orders *orders = FindOrders(registry, id);
            CC_CHECK(orders != nullptr && orders->attackMove); // FP march preserves attack-move
            const Mover *mover = FindMover(registry, id);
            if (mover != nullptr && (mover->hasMoveOrder || mover->hasPath))
            {
                ++ordered;
            }
            const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(mover->moveTarget));
            CC_CHECK(occ.CanEnter(map, dest, 1, 1, id, registry.Generation(id)));
        });
        CC_CHECK(ordered >= 3);
    }

    // --- unbound commander keeps legacy blind orders (null occ) ---
    {
        Registry registry;
        TileMap map(20, 15);
        ResourceNodes nodes;
        EventDispatcher events;
        OccupancyGrid occ(20, 15);
        ResourceSystem resources;
        AICommander ai(registry, map, nodes, events, 1, AIDifficulty::Easy, { 16, 9 },
                       { 1, 10 });
        ai.SetupBase();
        // NOTE: no SetOccupancy — legacy path.
        UnitFactory factory(registry, resources, events);
        for (int i = 0; i < 3; ++i)
        {
            factory.SpawnPrepaid(UnitType::RifleInfantry, 1,
                                 cc::ToRaylib(cc::TileToWorld(16 + i, 9)));
        }
        const Entity blocker = registry.Create();
        occ.ReserveFootprint({ 1, 10 }, 1, 1, blocker, registry.Generation(blocker));
        ai.Update(1.0f / 60.0f);
        CC_CHECK(ai.WavesLaunched() == 1);
        // Blind march drives straight at the slot even though it is occupied.
        bool droveAtBlocker = false;
        registry.Each<Unit>([&](Entity id, const Unit &unit) {
            if (id == blocker || unit.teamID != 1 || unit.health <= 0.0f ||
                unit.type == UnitType::Engineer)
            {
                return;
            }
            const Orders *orders = FindOrders(registry, id);
            CC_CHECK(orders != nullptr && orders->attackMove);
            const cc::IVec2 dest = cc::WorldToTile(cc::ToGlm(FindMover(registry, id)->moveTarget));
            if (dest == cc::IVec2(1, 10) || dest == cc::IVec2(2, 10))
            {
                droveAtBlocker = true;
            }
        });
        CC_CHECK(droveAtBlocker);
    }
}
