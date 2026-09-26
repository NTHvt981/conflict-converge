// Unit tests for save/load (JSON roundtrip, tolerant decode, rejection paths).

#include "test_harness.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "world/FogOfWar.h" // fixture carries fog memory for the team_fog roundtrip
#include "app/ui/GameCamera.h"
#include "economy/Nodes.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "app/save/SaveGame.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "units/UnitStats.h"
#include "app/save/SaveWire.h" // wire structs for the version-99 probe

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstdint> // INT32_MAX crafted-save probe
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

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
    scout.type = UnitType::RifleInfantry;
    ApplyBaseStats(scout);
    scout.teamID = 0;
    scout.position = cc::ToRaylib(cc::TileToWorld(2, 2));
    scout.isSelected = true;
    scout.health = 73.5f;
    scout.sightRange = 128.0f; // deterministic reveal for the team_fog roundtrip
    const Entity scoutId = src.registry.Create();
    src.registry.Add(scoutId, scout);
    CombatState scoutCombat;
    scoutCombat.cooldown = 0.2f;
    scoutCombat.lastDamageTaken = 12.0f;
    scoutCombat.hitFlashTime = 0.1f;
    src.registry.Add(scoutId, scoutCombat);

    Unit hunter;
    hunter.type = UnitType::LightTank;
    ApplyBaseStats(hunter);
    hunter.teamID = 1;
    hunter.position = cc::ToRaylib(cc::TileToWorld(6, 2));
    hunter.state = UnitState::Moving;
    const Entity hunterId = src.registry.Create();
    src.registry.Add(hunterId, hunter);
    CombatState hunterCombat;
    hunterCombat.target = scoutId; // cross-entity reference must survive the trip
    hunterCombat.phase = AttackPhase::WindUp;
    hunterCombat.phaseTime = 0.05f;
    src.registry.Add(hunterId, hunterCombat);
    Mover hunterMover;
    hunterMover.hasMoveOrder = true;
    hunterMover.moveTarget = cc::ToRaylib(cc::TileToWorld(3, 3));
    hunterMover.path = { { 5, 2 }, { 4, 2 }, { 3, 3 } };
    hunterMover.pathNext = 1;
    hunterMover.hasPath = true;
    src.registry.Add(hunterId, hunterMover);

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
    CC_CHECK(ls->health == 73.5f); // JSON round-trips exact floats
    const CombatState *loadedScoutCombat = FindCombatState(dst.registry, loadedScout);
    CC_CHECK(Near(loadedScoutCombat->lastDamageTaken, 12.0f) &&
             Near(loadedScoutCombat->hitFlashTime, 0.1f));
    CC_CHECK(Near(loadedScoutCombat->cooldown, 0.2f));
    const CombatState *loadedHunterCombat = FindCombatState(dst.registry, loadedHunter);
    CC_CHECK(loadedHunterCombat->target == loadedScout); // remapped to the fresh scout ID
    const Mover *loadedMover = FindMover(dst.registry, loadedHunter);
    CC_CHECK(loadedMover != nullptr && loadedMover->hasMoveOrder && loadedMover->hasPath &&
             loadedMover->pathNext == 1 && loadedMover->path.size() == 3);
    CC_CHECK(loadedMover->path[0] == cc::IVec2(5, 2) && loadedMover->path[2] == cc::IVec2(3, 3));
    CC_CHECK(lh->state == UnitState::Moving &&
             loadedHunterCombat->phase == AttackPhase::WindUp);
    CC_CHECK(Near(loadedHunterCombat->phaseTime, 0.05f));
    CC_CHECK(loadedHunterCombat->phaseTime == 0.05f); // JSON round-trips exact floats

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
        // Old CCPB (protobuf-era) saves are rejected at the magic check.
        std::ofstream old(path, std::ios::binary | std::ios::trunc);
        const char legacy[] = { 'C', 'C', 'P', 'B', 2, 0, 0, 0 };
        old.write(legacy, sizeof(legacy));
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    {
        // Unsupported save_version inside a well-formed payload.
        SaveGameData future;
        future.saveVersion = 99;
        std::ostringstream payload(std::ios::binary);
        {
            cereal::JSONOutputArchive ar(payload);
            ar(future);
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write("CCJ3", 4);
        const std::string bytes = payload.str();
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    {
        // Previous binary format (CCB2 magic) is rejected at the magic check.
        std::ofstream old(path, std::ios::binary | std::ios::trunc);
        const char legacy[] = { 'C', 'C', 'B', '2', 2, 0, 0, 0, 0, 0, 0, 0 };
        old.write(legacy, sizeof(legacy));
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

    {
        // Crafted save: building tile_x at INT32_MAX smuggled through the
        // real encode path (hand-added to the registry, bypassing placement
        // validation). The old `tile_x + fp.x > mapWidth` check
        // signed-overflows and wraps negative, passing validation to smuggle
        // an out-of-range tile; the subtraction form must reject it.
        Fixture trap;
        Building evil;
        evil.type = BuildingType::Base;
        evil.state = BuildingState::Operational;
        evil.teamID = 0;
        evil.tileX = INT32_MAX;
        evil.tileY = 0;
        evil.health = BuildingMaxHealth(BuildingType::Base);
        evil.maxHealth = evil.health;
        trap.registry.Add(trap.registry.Create(), evil);
        CC_CHECK(SaveWorld(trap.State(), path));
    }
    CC_CHECK(!LoadWorld(victimState, path));
    CC_CHECK(victim.resources.iron == 42 && victim.registry.EntityCount() == 0);

    // --- tolerant decode: missing keys take defaults, unknown keys ignored ---
    // A hand-written sparse v3 save (map + one partial unit, future and
    // mystery keys sprinkled in) must load with defaults elsewhere. Note
    // the "value0" root: cereal auto-names the unnamed top-level struct.
    {
        std::ostringstream sparse;
        sparse << "{\"value0\":{\"saveVersion\":3,"
               << "\"futureSection\":{\"whatever\":1},"
               << "\"map\":{\"width\":20,\"height\":15,\"terrain\":[";
        for (int i = 0; i < 300; ++i)
        {
            sparse << (i == 0 ? "0" : ",0");
        }
        sparse << "]},"
               << "\"units\":[{"
               << "\"type\":0,\"team\":0,\"health\":50.0,"
               << "\"position\":{\"x\":128.0,\"y\":128.0},"
               << "\"mysteryField\":42"
               << "}]}}";
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write("CCJ3", 4);
        const std::string bytes = sparse.str();
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        Fixture sparseWorld;
        CC_CHECK(LoadWorld(sparseWorld.State(), path));
        CC_CHECK(sparseWorld.registry.EntityCount() == 1);
        Entity only = kInvalidEntity;
        sparseWorld.registry.Each<Unit>([&](Entity id, const Unit &) { only = id; });
        const Unit *u = sparseWorld.registry.Get<Unit>(only);
        CC_CHECK(u != nullptr);
        if (u == nullptr)
        {
            return;
        }
        CC_CHECK(u->type == UnitType::RifleInfantry && u->teamID == 0);
        CC_CHECK(u->health == 50.0f);
        CC_CHECK(FindCombatState(sparseWorld.registry, only)->cooldown == 0.0f &&
                 FindMover(sparseWorld.registry, only) != nullptr &&
                 !FindMover(sparseWorld.registry, only)->hasMoveOrder &&
                 !FindMover(sparseWorld.registry, only)->hasPath);
        CC_CHECK(u->attackPower == 0 && u->attackRange == 0); // absent: zero, not table
        CC_CHECK(u->position.x == 128.0f && u->position.y == 128.0f);
        CC_CHECK(sparseWorld.resources.iron == 0 && sparseWorld.nodes.Count() == 0);
    }

    // --- QoL replay sequencing: frames save/load in order with motion ---
    {
        const std::string dir =
            (std::filesystem::temp_directory_path() / "cc_replay_test").string();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        Fixture actor;
        Unit walker;
        walker.type = UnitType::RifleInfantry;
        ApplyBaseStats(walker);
        walker.teamID = 0;
        walker.speed = 64.0f;
        walker.position = cc::ToRaylib(cc::TileToWorld(1, 1));
        const Entity walkerId = actor.registry.Create();
        actor.registry.Add(walkerId, walker);
        IssueMoveOrder(*actor.registry.Get<Unit>(walkerId), GetOrders(actor.registry, walkerId),
                       GetMover(actor.registry, walkerId),
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

    // --- M2 extension round-trip: turret facing + cargo manifest + embarked ---
    {
        Fixture carrier;
        Unit hull;
        hull.type = UnitType::HeavyTank;
        ApplyBaseStats(hull);
        hull.teamID = 0;
        hull.position = cc::ToRaylib(cc::TileToWorld(2, 2));
        hull.facing = Facing::Left; // G1: body facing persists alongside turret
        const Entity carrierId = carrier.registry.Create();
        carrier.registry.Add(carrierId, hull);
        Turret turret;
        turret.facing = 1.25f;
        turret.turnRate = 2.0f;
        carrier.registry.Add(carrierId, turret);
        Cargo cargo;
        cargo.capacity = 6;
        carrier.registry.Add(carrierId, cargo);

        Unit rider;
        rider.type = UnitType::RifleInfantry;
        ApplyBaseStats(rider);
        rider.teamID = 0;
        rider.position = cc::ToRaylib(cc::TileToWorld(2, 2));
        const Entity riderId = carrier.registry.Create();
        carrier.registry.Add(riderId, rider);
        EmbarkedOn ride;
        ride.carrier = carrierId;
        carrier.registry.Add(riderId, ride);
        carrier.registry.Get<Cargo>(carrierId)->passengers.push_back(riderId);

        const std::string xpath =
            (std::filesystem::temp_directory_path() / "cc_save_m2.ccpb").string();
        CC_CHECK(SaveWorld(carrier.State(), xpath));
        Fixture landed;
        CC_CHECK(LoadWorld(landed.State(), xpath));
        CC_CHECK(landed.registry.EntityCount() == 2);
        Entity loadedCarrier = kInvalidEntity, loadedRider = kInvalidEntity;
        landed.registry.Each<Unit>([&](Entity id, const Unit &u) {
            if (u.type == UnitType::HeavyTank)
            {
                loadedCarrier = id;
            }
            else
            {
                loadedRider = id;
            }
        });
        CC_CHECK(loadedCarrier != kInvalidEntity && loadedRider != kInvalidEntity);
        const Turret *lt = landed.registry.Get<Turret>(loadedCarrier);
        CC_CHECK(lt != nullptr && Near(lt->facing, 1.25f));
        CC_CHECK(landed.registry.Get<Unit>(loadedCarrier)->facing == Facing::Left);
        const Cargo *lc = landed.registry.Get<Cargo>(loadedCarrier);
        CC_CHECK(lc != nullptr && lc->passengers.size() == 1 &&
                 lc->passengers[0] == loadedRider);
        const EmbarkedOn *lr = landed.registry.Get<EmbarkedOn>(loadedRider);
        CC_CHECK(lr != nullptr && lr->carrier == loadedCarrier);
        std::remove(xpath.c_str());
    }

    std::remove(path.c_str());
}
