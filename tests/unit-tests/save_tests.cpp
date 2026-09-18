// Unit tests for save/load (binary roundtrip + rejection paths).

#include "test_harness.h"

#include "Building.h"
#include "FogOfWar.h" // fixture carries fog memory for the team_fog roundtrip
#include "GameCamera.h"
#include "Nodes.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "TileMap.h"
#include "Unit.h"
#include "UnitStats.h"
#include "../../src/game/private/app/savegame.pb.h" // private/ is NOT on the test include path

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace
{

struct Fixture
{
    Registry registry;
    ResourceSystem resources;
    TileMap map{ 20, 15 };
    GameCamera camera;
    ResourceNodes nodes;
    FogOfWar fog;

    Fixture()
    {
        fog.Resize(20, 15);
    }

    WorldState State()
    {
        return { &registry, &resources, &map, &camera, &nodes, &fog };
    }
};

std::string ScratchPath()
{
    return (std::filesystem::temp_directory_path() / "cc_save_roundtrip.ccpb").string();
}

bool Near(float a, float b)
{
    return CcNear(a, b);
}

} // namespace

void RunSaveGameTests()
{
    // --- build a representative world ---
    Fixture src;
    src.resources.AddIron(1234);
    src.resources.AddOil(567);
    src.resources.TickIncome(0.4f, 0.4f, 1.0f); // leaves fractional carry banked
    src.camera.view.target = { 320.0f, 240.0f };
    src.camera.view.offset = { 400.0f, 225.0f };
    src.camera.view.rotation = 0.0f;
    src.camera.view.zoom = 1.5f;
    src.map.Set({ 6, 3 }, TerrainType::Water);
    PlaceBuilding(src.registry, src.map, BuildingType::Base, 0, 1, 10);
    PlaceBuilding(src.registry, src.map, BuildingType::Factory, 1, 10, 10);
    UpdateBuildingConstruction(src.registry, 20.0f); // save Operational structures
    src.nodes.SpawnNode(src.map, ResourceKind::Iron, { 15, 3 }, 200.0f, 10.0f);
    src.nodes.SpawnNode(src.map, ResourceKind::Oil, { 15, 12 }, 150.0f, 10.0f);

    Unit scout;
    scout.type = UnitType::Infantry;
    ApplyBaseStats(scout);
    scout.teamID = 0;
    scout.position = cc::ToRaylib(cc::TileToWorld(2, 2));
    scout.isSelected = true;
    scout.health = 73.5f;
    scout.cooldown = 0.2f;
    scout.sightRange = 128.0f; // deterministic reveal for the team_fog roundtrip
    scout.lastDamageTaken = 12.0f;
    scout.hitFlashTime = 0.1f;
    const Entity scoutId = src.registry.Create();
    src.registry.Add(scoutId, scout);

    Unit hunter;
    hunter.type = UnitType::LightTank;
    ApplyBaseStats(hunter);
    hunter.teamID = 1;
    hunter.position = cc::ToRaylib(cc::TileToWorld(6, 2));
    hunter.target = scoutId; // cross-entity reference must survive the trip
    hunter.hasMoveOrder = true;
    hunter.moveTarget = cc::ToRaylib(cc::TileToWorld(3, 3));
    hunter.path = { { 5, 2 }, { 4, 2 }, { 3, 3 } };
    hunter.pathNext = 1;
    hunter.hasPath = true;
    hunter.state = UnitState::Moving;
    hunter.phase = AttackPhase::WindUp;
    hunter.phaseTime = 0.05f;
    const Entity hunterId = src.registry.Create();
    src.registry.Add(hunterId, hunter);

    const std::string path = ScratchPath();
    src.fog.Recompute(src.registry); // bank explored memory for team_fog
    CC_CHECK(SaveWorld(src.State(), path));

    // --- load into a differently-shaped world (tests Resize + Clear) ---
    Fixture dst;
    dst.map = TileMap(4, 4);
    Unit junk;
    dst.registry.Add(dst.registry.Create(), junk);
    CC_CHECK(LoadWorld(dst.State(), path));

    CC_CHECK(dst.resources.iron == 1234);
    CC_CHECK(dst.resources.oil == 567);
    CC_CHECK(Near(dst.resources.IronCarry(), src.resources.IronCarry()));
    CC_CHECK(Near(dst.resources.OilCarry(), src.resources.OilCarry()));
    CC_CHECK(Near(dst.camera.view.target.x, 320.0f));
    CC_CHECK(Near(dst.camera.view.target.y, 240.0f));
    CC_CHECK(Near(dst.camera.view.offset.x, 400.0f));
    CC_CHECK(Near(dst.camera.view.zoom, 1.5f));
    CC_CHECK(dst.map.Width() == 20 && dst.map.Height() == 15);
    CC_CHECK(dst.map.Get({ 6, 3 }) == TerrainType::Water);
    CC_CHECK(dst.map.Get({ 1, 10 }) == TerrainType::Building);
    CC_CHECK(dst.map.Get({ 0, 0 }) == TerrainType::Grass);
    CC_CHECK(dst.fog.IsExplored(0, { 2, 2 })); // team_fog survived the trip
    CC_CHECK(!dst.fog.IsExplored(0, { 19, 14 }));

    CC_CHECK(dst.registry.EntityCount() == 4); // 2 units + 2 buildings, junk cleared
    CC_CHECK(dst.nodes.Count() == 2);
    const ResourceNode *iron = dst.nodes.FindAt({ 15, 3 });
    CC_CHECK(iron != nullptr && iron->kind == ResourceKind::Iron);
    CC_CHECK(Near(iron->amount, 200.0f) && Near(iron->maxAmount, 200.0f));

    // Find the loaded pair by team and verify the target remap.
    Entity loadedScout = kInvalidEntity, loadedHunter = kInvalidEntity;
    dst.registry.Each<Unit>([&](Entity id, const Unit &u) {
        if (u.teamID == 0)
        {
            loadedScout = id;
        }
        else
        {
            loadedHunter = id;
        }
    });
    CC_CHECK(loadedScout != kInvalidEntity && loadedHunter != kInvalidEntity);
    const Unit *ls = dst.registry.Get<Unit>(loadedScout);
    const Unit *lh = dst.registry.Get<Unit>(loadedHunter);
    CC_CHECK(Near(ls->health, 73.5f) && ls->isSelected);
    CC_CHECK(Near(ls->lastDamageTaken, 12.0f) && Near(ls->hitFlashTime, 0.1f));
    CC_CHECK(Near(ls->cooldown, 0.2f));
    CC_CHECK(lh->target == loadedScout); // remapped to the fresh scout ID
    CC_CHECK(lh->hasMoveOrder && lh->hasPath && lh->pathNext == 1 && lh->path.size() == 3);
    CC_CHECK(lh->path[0] == cc::IVec2(5, 2) && lh->path[2] == cc::IVec2(3, 3));
    CC_CHECK(lh->state == UnitState::Moving && lh->phase == AttackPhase::WindUp);
    CC_CHECK(Near(lh->phaseTime, 0.05f));

    int bases = 0, factories = 0;
    dst.registry.Each<Building>([&](Entity, const Building &b) {
        bases += (b.type == BuildingType::Base) ? 1 : 0;
        factories += (b.type == BuildingType::Factory) ? 1 : 0;
        CC_CHECK(b.health > 0.0f && b.health <= b.maxHealth); // HP roundtrips
    });
    CC_CHECK(bases == 1 && factories == 1);

    // --- loaded world keeps simulating (save is not a freeze-frame) ---
    const float hpBefore = lh->health;
    (void)hpBefore;
    UpdateUnit(loadedHunter, dst.registry, dst.map, 1.0f / 60.0f);
    CC_CHECK(dst.registry.IsAlive(loadedHunter));

    // --- rejection paths leave the destination untouched ---
    Fixture victim;
    victim.resources.AddIron(42);
    const WorldState victimState = victim.State();
    CC_CHECK(!LoadWorld(victimState, path + ".missing"));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    {
        // Bad magic.
        std::ofstream bad(path, std::ios::binary | std::ios::trunc);
        const char junk2[] = { 'N', 'O', 'P', 'E', 0, 0, 0, 0 };
        bad.write(junk2, sizeof(junk2));
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    {
        // Old CCSV custom-binary files are not loadable (no back-compat shim).
        std::ofstream old(path, std::ios::binary | std::ios::trunc);
        const char legacy[] = { 'C', 'C', 'S', 'V', 1, 0, 0, 0 };
        old.write(legacy, sizeof(legacy));
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    {
        // Unsupported save_version inside a well-formed protobuf payload.
        cc::save::SaveGame future;
        future.set_save_version(99);
        std::string payload;
        CC_CHECK(future.SerializeToString(&payload));
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write("CCPB", 4);
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    {
        // Truncated mid-stream (valid header, cut off early).
        Fixture tiny;
        CC_CHECK(SaveWorld(tiny.State(), path));
        std::error_code ec;
        std::filesystem::resize_file(path, 20, ec);
        CC_CHECK(!ec);
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    // --- QoL replay sequencing: frames save/load in order with motion ---
    {
        const std::string dir =
            (std::filesystem::temp_directory_path() / "cc_replay_test").string();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        Fixture actor;
        Unit walker;
        walker.type = UnitType::Infantry;
        ApplyBaseStats(walker);
        walker.teamID = 0;
        walker.speed = 64.0f;
        walker.position = cc::ToRaylib(cc::TileToWorld(1, 1));
        const Entity walkerId = actor.registry.Create();
        actor.registry.Add(walkerId, walker);
        IssueMoveOrder(*actor.registry.Get<Unit>(walkerId),
                       cc::ToRaylib(cc::TileToWorld(5, 1)));

        float expectedX[3] = {};
        for (int frame = 0; frame < 3; ++frame)
        {
            UpdateUnit(walkerId, actor.registry, actor.map, 1.0f);
            expectedX[frame] = actor.registry.Get<Unit>(walkerId)->position.x;
            CC_CHECK(SaveWorld(actor.State(), ReplayFramePath(dir, frame)));
        }
        CC_CHECK(ReplayFrameCount(dir) == 3);
        CC_CHECK(ReplayFramePath(dir, 0).find("replay_0000.ccpb") != std::string::npos);

        // Reload each frame in order: positions match the recorded motion.
        for (int frame = 0; frame < 3; ++frame)
        {
            Fixture viewer;
            CC_CHECK(LoadWorld(viewer.State(), ReplayFramePath(dir, frame)));
            Entity seen = kInvalidEntity;
            viewer.registry.Each<Unit>([&](Entity id, const Unit &) { seen = id; });
            CC_CHECK(seen != kInvalidEntity);
            CC_CHECK(Near(viewer.registry.Get<Unit>(seen)->position.x, expectedX[frame]));
        }
        // A gap stops the count (stops at first missing frame).
        std::filesystem::remove(ReplayFramePath(dir, 1), ec);
        CC_CHECK(ReplayFrameCount(dir) == 1);
        std::filesystem::remove_all(dir, ec);
    }

    std::remove(path.c_str());
}
