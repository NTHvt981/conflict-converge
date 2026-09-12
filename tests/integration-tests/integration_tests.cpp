// M7 integration tests: multi-system scenarios driven headless through the
// same per-frame calls main.cpp makes (UpdateUnit, death sweep, economy,
// production, save/load). Unit suites cover components; these cover play.

#include "../unit-tests/test_harness.h"

#include "Building.h"
#include "Event.h"
#include "GameCamera.h" // WorldState carries the camera
#include "MathUtils.h"
#include "Nodes.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitFactory.h"
#include "UnitStats.h"

#include <filesystem>
#include <vector>

namespace
{

constexpr float kDt = 1.0f / 60.0f;

int CountTeam(Registry &registry, int team)
{
    int count = 0;
    registry.Each<Unit>([&](Entity, const Unit &unit) {
        if (unit.health > 0.0f && unit.teamID == team)
        {
            ++count;
        }
    });
    return count;
}

void SimulateCombatFrames(Registry &registry, TileMap &map, UnitFactory &factory, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        registry.Each<Unit>([&](Entity id, Unit &unit) { UpdateUnit(id, registry, map, kDt); });
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
}

// Place asymmetric squads (3 vs 2) facing off at 80% of the shorter of
// sight/range (data-driven: no hardcoded stat assumptions). The extra body
// guarantees a decisive winner — mirror matches can mutually annihilate.
void DeployFaceoff(Registry &registry, UnitFactory &factory, UnitType type)
{
    const UnitStats stats = BaseStats(type);
    const float gap = (stats.sightRange < stats.attackRange ? stats.sightRange : stats.attackRange) * 0.8f;
    for (int i = 0; i < 3; ++i)
    {
        const float y = 320.0f + static_cast<float>(i) * 64.0f;
        factory.Spawn(type, 0, { 320.0f, y });
        if (i < 2)
        {
            factory.Spawn(type, 1, { 320.0f + gap, y });
        }
    }
}

} // namespace

void RunIntegrationTests()
{
    // --- squad battle resolves: one side wipes the other in frame budget ---
    {
        Registry registry;
        ResourceSystem resources;
        resources.AddIron(100000);
        resources.AddOil(100000);
        EventDispatcher events;
        UnitFactory factory(registry, resources, events);
        TileMap map(20, 15);
        DeployFaceoff(registry, factory, UnitType::Infantry);

        int frames = 0;
        while (CountTeam(registry, 0) > 0 && CountTeam(registry, 1) > 0 && frames < 3600)
        {
            SimulateCombatFrames(registry, map, factory, 60);
            frames += 60;
        }
        CC_CHECK(CountTeam(registry, 0) > 0); // favored side standing
        CC_CHECK(CountTeam(registry, 1) == 0); // underdogs wiped
        CC_CHECK(frames < 3600);               // it terminated, not timed out
    }

    // --- economy cycle: base trickle + harvest + queue produce a unit ---
    {
        Registry registry;
        ResourceSystem resources;
        resources.AddIron(500);
        resources.AddOil(200);
        EventDispatcher events;
        UnitFactory factory(registry, resources, events);
        TileMap map(20, 15);
        PlaceBuilding(registry, map, BuildingType::Base, 0, 1, 10);
        ResourceNodes nodes;
        nodes.SpawnNode(map, ResourceKind::Iron, { 15, 3 }, 10000.0f, 60.0f);
        factory.Spawn(UnitType::Engineer, 0, cc::ToRaylib(cc::TileToWorld(15, 3)));
        ProductionQueue queue;
        CC_CHECK(queue.Enqueue(resources, UnitType::Infantry));
        const std::size_t before = registry.EntityCount();
        const Vector2 rally = cc::ToRaylib(cc::TileToWorld(13, 11));

        for (int i = 0; i < 1200; ++i) // 20 simulated seconds
        {
            UpdateBaseIncome(registry, resources, kDt, 0);
            nodes.Update(kDt);
            nodes.GatherTick(registry, resources, kDt);
            queue.Update(factory, 0, rally, kDt);
            registry.Each<Unit>([&](Entity id, Unit &unit) { UpdateUnit(id, registry, map, kDt); });
        }
        CC_CHECK(queue.Empty());                        // order rolled off the line
        CC_CHECK(registry.EntityCount() > before);      // reinforcement arrived
        CC_CHECK(resources.iron > 500 - CostOf(UnitType::Infantry).iron); // income outpaced cost
    }

    // --- mid-combat save/load: snapshot, resume, sim completes ---
    {
        Registry registry;
        ResourceSystem resources;
        resources.AddIron(100000);
        resources.AddOil(100000);
        EventDispatcher events;
        UnitFactory factory(registry, resources, events);
        TileMap map(20, 15);
        GameCamera camera;
        ResourceNodes nodes;
        DeployFaceoff(registry, factory, UnitType::LightTank);
        SimulateCombatFrames(registry, map, factory, 60);

        const std::string path =
            (std::filesystem::temp_directory_path() / "cc_integration_midfight.ccpb").string();
        WorldState src{ &registry, &resources, &map, &camera, &nodes };
        CC_CHECK(SaveWorld(src, path));

        Registry registry2;
        ResourceSystem resources2;
        TileMap map2(4, 4);
        GameCamera camera2;
        ResourceNodes nodes2;
        WorldState dst{ &registry2, &resources2, &map2, &camera2, &nodes2 };
        CC_CHECK(LoadWorld(dst, path));
        CC_CHECK(registry2.EntityCount() == registry.EntityCount());

        UnitFactory factory2(registry2, resources2, events);
        SimulateCombatFrames(registry2, map2, factory2, 600);
        // No crash, no residue: every remaining unit is alive and placed.
        registry2.Each<Unit>([&](Entity, const Unit &unit) {
            CC_CHECK(unit.health > 0.0f);
            CC_CHECK(map2.InBounds(cc::WorldToTile(cc::ToGlm(unit.position))));
        });
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}
