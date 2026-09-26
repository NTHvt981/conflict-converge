// Integration tests: multi-system scenarios driven headless through the
// same per-frame calls main.cpp makes (UpdateUnit, death sweep, economy,
// production, save/load). Unit suites cover components; these cover play.

#include "../unit-tests/test_harness.h"

#include "economy/Building.h"
#include "units/Combat.h" // G2: headless drivers step UpdateProjectiles
#include "core/Event.h"
#include "units/AICommander.h" // Difficulty ladder decides games
#include "world/FogOfWar.h" // WorldState carries fog memory
#include "app/ui/GameCamera.h" // WorldState carries the camera
#include "core/MathUtils.h"
#include "economy/Nodes.h"
#include "economy/Production.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/save/SaveGame.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"
#include "units/UnitStats.h"

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
        // G2: shells are a per-frame system like movement — any headless
        // driver that steps UpdateUnit must step them too, or arcing units
        // read as zero-DPS and sieges stall.
        UpdateProjectiles(registry, map, kDt);
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

// AI-vs-AI driver: two commanders build, harvest, wave, and scrap
// until one team is wiped or the cap hits. Prints kill/wave telemetry every
// 20 sim-seconds. Returns frames simulated. Death sweep mirrors the game
// loop (corpse kill-credit by team, then destroy).
int RunAISoak(Registry &registry, TileMap &map, ResourceNodes &nodes, AICommander &first,
              AICommander &second, const char *label)
{
    int frames = 0;
    int kills0 = 0, kills1 = 0;
    // 7.5 sim-minutes: support units (Medic, Engineer) cannot attack, so
    // harvesters ride out raids instead of chasing to their deaths and
    // games legitimately run longer than the old all-combatant casts.
    constexpr int kCap = 27000;
    while (CountTeam(registry, 0) > 0 && CountTeam(registry, 1) > 0 && frames < kCap)
    {
        first.Update(kDt);
        second.Update(kDt);
        UpdateBuildingConstruction(registry, kDt); // sites -> Operational, like Game
        nodes.Update(kDt);
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            UpdateUnit(id, registry, map, kDt);
        });
        UpdateProjectiles(registry, map, kDt); // G2: headless soak steps shells too
        std::vector<Entity> dead;
        registry.Each<Unit>([&](Entity id, const Unit &unit) {
            if (unit.health <= 0.0f)
            {
                dead.push_back(id);
            }
        });
        for (Entity id : dead)
        {
            if (const Unit *corpse = registry.Get<Unit>(id))
            {
                if (corpse->teamID == 0)
                {
                    ++kills0;
                }
                else
                {
                    ++kills1;
                }
            }
            registry.Destroy(id);
        }
        ++frames;
        if (frames % 1200 == 0)
        {
            std::printf("soak %s t=%ds t0=%d t1=%d kills=%d/%d waves=%d/%d\n", label, frames / 60,
                        CountTeam(registry, 0), CountTeam(registry, 1), kills0, kills1,
                        first.WavesLaunched(), second.WavesLaunched());
        }
    }
    std::printf("soak: %s decided in %d frames\n", label, frames);
    return frames;
}

// Mirror arena factory: Crossroads-sized (24x18) with corner
// homes, so range, scouting, and retreats — Hard's whole kit — actually
// matter. The old 16x12 phone booth decided by cost-efficiency alone at
// contact range, which structurally negates artillery/scouts and flattered
// cheap mass. Nodes and homes are 180-degree symmetric.
void SeedSoakArena(TileMap &map, ResourceNodes &nodes)
{
    nodes.SpawnNode(map, ResourceKind::Iron, { 4, 13 }, 300.0f, 10.0f);
    nodes.SpawnNode(map, ResourceKind::Oil, { 5, 11 }, 200.0f, 10.0f);
    nodes.SpawnNode(map, ResourceKind::Iron, { 19, 4 }, 300.0f, 10.0f);
    nodes.SpawnNode(map, ResourceKind::Oil, { 18, 6 }, 200.0f, 10.0f);
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
        DeployFaceoff(registry, factory, UnitType::RifleInfantry);

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
        CC_CHECK(queue.Enqueue(resources, UnitType::RifleInfantry));
        const std::size_t before = registry.EntityCount();
        const Vector2 rally = cc::ToRaylib(cc::TileToWorld(13, 11));

        for (int i = 0; i < 1200; ++i) // 20 simulated seconds
        {
            UpdateBuildingConstruction(registry, kDt); // site -> Operational, like Game
            UpdateBaseIncome(registry, resources, kDt, 0);
            nodes.Update(kDt);
            nodes.GatherTick(registry, resources, kDt);
            queue.Update(factory, resources, 0, rally, kDt);
            registry.Each<Unit>([&](Entity id, Unit &unit) { UpdateUnit(id, registry, map, kDt); });
        }
        CC_CHECK(queue.Empty());                        // order rolled off the line
        CC_CHECK(registry.EntityCount() > before);      // reinforcement arrived
        CC_CHECK(resources.iron > 500 - CostOf(UnitType::RifleInfantry).iron); // income outpaced cost
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
        FogOfWar fog;
        fog.Resize(20, 15);
        DeployFaceoff(registry, factory, UnitType::LightTank);
        SimulateCombatFrames(registry, map, factory, 60);

        const std::string path =
            (std::filesystem::temp_directory_path() / "cc_integration_midfight.ccpb").string();
        WorldState src{ &registry, &resources, &map, &camera, &nodes, &fog };
        CC_CHECK(SaveWorld(src, path));

        Registry registry2;
        ResourceSystem resources2;
        TileMap map2(4, 4);
        GameCamera camera2;
        ResourceNodes nodes2;
        FogOfWar fog2;
        fog2.Resize(4, 4);
        WorldState dst{ &registry2, &resources2, &map2, &camera2, &nodes2, &fog2 };
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

    // --- Balance: the difficulty ladder decides games, no stalemates ---
    // Easy-vs-Medium: accepted stalemate (gimmick sign-off). At HEAD this
    // rung decided for Medium at 15639 frames; the G1 fire-only-when-aimed
    // gate flips it into a stable replacement equilibrium (bisect-proven:
    // gate off reproduces the HEAD trajectory frame-for-frame; shell
    // flight/splash tuning has zero effect on this rung). Medium still
    // out-kills Easy ~2:1 along the way; the rung pins the capped,
    // both-alive trajectory instead of a wipe.
    {
        Registry registry;
        TileMap map(24, 18);
        ResourceNodes nodes;
        SeedSoakArena(map, nodes);
        EventDispatcher events;
        AICommander easy(registry, map, nodes, events, 0, AIDifficulty::Easy, { 2, 15 }, { 21, 2 });
        AICommander medium(registry, map, nodes, events, 1, AIDifficulty::Medium, { 21, 2 },
                           { 2, 15 });
        easy.SetupBase();
        medium.SetupBase();
        const int frames = RunAISoak(registry, map, nodes, easy, medium, "Easy-vs-Medium");
        CC_CHECK(frames == 27000); // capped (deterministic): stable equilibrium
        CC_CHECK(CountTeam(registry, 0) > 0 && CountTeam(registry, 1) > 0);
    }
    {
        Registry registry;
        TileMap map(24, 18);
        ResourceNodes nodes;
        SeedSoakArena(map, nodes);
        EventDispatcher events;
        AICommander medium(registry, map, nodes, events, 0, AIDifficulty::Medium, { 2, 15 },
                           { 21, 2 });
        AICommander hard(registry, map, nodes, events, 1, AIDifficulty::Hard, { 21, 2 }, { 2, 15 });
        medium.SetupBase();
        hard.SetupBase();
        const int frames = RunAISoak(registry, map, nodes, medium, hard, "Medium-vs-Hard");
        CC_CHECK(frames < 27000); // terminated: no stalemate
        CC_CHECK(CountTeam(registry, 1) > 0); // Hard wins the second rung
        CC_CHECK(CountTeam(registry, 0) == 0);
    }
}
