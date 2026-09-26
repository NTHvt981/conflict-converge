#include "app/SaveGame.h"

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "SaveWire.h"

#include "economy/Building.h"
#include "core/CcAssert.h"
#include "units/Extensions.h"
#include "world/FogOfWar.h"
#include "app/GameCamera.h"
#include "economy/Nodes.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "world/TileMap.h"
#include "units/Unit.h"
#include "app/UnitConfig.h"

namespace
{

constexpr char kMagic[4] = { 'C', 'C', 'J', '3' };
constexpr std::int32_t kMaxMapTiles = 1024 * 1024;
constexpr std::int32_t kMaxUnits = 100000;
constexpr std::int32_t kMaxBuildings = 100000;
constexpr std::int32_t kMaxNodes = 100000;
constexpr std::uint32_t kMaxPathNodes = 100000;
constexpr std::int32_t kMaxFogTeams = 16;
constexpr int kMaxTeamID = 1;

static_assert(static_cast<int>(ArmorType::Count) == 3, "ArmorType grew: review save decode");
static_assert(static_cast<int>(DamageType::Count) == 3, "DamageType grew: review save decode");
static_assert(static_cast<int>(UnitType::Count) == 9, "UnitType grew: review save decode");
static_assert(static_cast<int>(UnitState::Count) == 3, "UnitState grew: review save decode");
static_assert(static_cast<int>(AttackPhase::Count) == 3, "AttackPhase grew: review save decode");
static_assert(static_cast<int>(TerrainType::Count) == 5, "TerrainType grew: review save decode");
static_assert(static_cast<int>(BuildingType::Count) == 3, "BuildingType grew: review save decode");
static_assert(static_cast<int>(BuildingState::Count) == 3, "BuildingState grew: review save decode");
static_assert(static_cast<int>(ResourceKind::Count) == 2, "ResourceKind grew: review save decode");

void FillVec2(SaveVec2 &out, Vector2 v)
{
    out.x = v.x;
    out.y = v.y;
}

Vector2 ToVec2(const SaveVec2 &in)
{
    return { in.x, in.y };
}

void FillUnit(SaveUnit &out, const Registry &registry, Entity id, const Unit &u,
              const std::vector<Entity> &order)
{
    auto indexOf = [&](Entity e) -> std::int32_t {
        if (e == kInvalidEntity)
        {
            return -1;
        }
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            if (order[i] == e)
            {
                return static_cast<std::int32_t>(i);
            }
        }
        return -1;
    };    out.health = u.health;
    out.armorType = static_cast<std::int32_t>(u.armorType);
    out.damageType = static_cast<std::int32_t>(u.damageType);
    out.attackPower = u.attackPower;
    out.attackRange = u.attackRange;
    const CombatState *combat = FindCombatState(registry, id);
    out.cooldown = combat != nullptr ? combat->cooldown : 0.0f;
    out.cooldownTime = u.cooldownTime;
    out.phase = combat != nullptr ? static_cast<std::int32_t>(combat->phase) : 0;
    out.phaseTime = combat != nullptr ? combat->phaseTime : 0.0f;
    out.windupTime = u.windupTime;
    out.lastDamage = combat != nullptr ? combat->lastDamageTaken : 0.0f;
    out.hitFlash = combat != nullptr ? combat->hitFlashTime : 0.0f;
    out.speed = u.speed;
    out.sightRange = u.sightRange;
    FillVec2(out.position, u.position);
    FillVec2(out.velocity, u.velocity);
    out.selected = u.isSelected;
    out.team = u.teamID;
    out.type = static_cast<std::int32_t>(u.type);
    out.state = static_cast<std::int32_t>(u.state);
    out.targetIndex = indexOf(combat != nullptr ? combat->target : kInvalidEntity);
    const Mover *mover = FindMover(registry, id);
    FillVec2(out.moveTarget, mover != nullptr ? mover->moveTarget : Vector2{});
    out.hasMoveOrder = mover != nullptr && mover->hasMoveOrder;
    if (mover != nullptr)
    {
        for (const cc::IVec2 &step : mover->path)
        {
            SaveIVec2 dst;
            dst.x = step.x;
            dst.y = step.y;
            out.path.push_back(dst);
        }
        out.pathNext = static_cast<std::uint32_t>(mover->pathNext);
        out.hasPath = mover->hasPath;
    }
    else
    {
        out.pathNext = 0;
        out.hasPath = false;
    }
    out.hasTurret = false;
    out.turretFacing = 0.0f;
    out.facing = static_cast<std::int32_t>(u.facing);
    if (const Turret *turret = registry.Get<Turret>(id))
    {
        out.hasTurret = true;
        out.turretFacing = turret->facing;
    }
    out.cargoManifest.clear();
    if (const Cargo *cargo = registry.Get<Cargo>(id))
    {
        for (const Entity passenger : cargo->passengers)
        {
            const std::int32_t idx = indexOf(passenger);
            if (idx >= 0)
            {
                out.cargoManifest.push_back(idx);
            }
        }
    }
    out.embarkedOn = -1;
    if (const EmbarkedOn *ride = registry.Get<EmbarkedOn>(id))
    {
        out.embarkedOn = indexOf(ride->carrier);
    }
}

struct SavedUnit
{
    Unit unit;
    Mover mover;
    CombatState combat;
    std::int32_t targetIndex = -1;
    bool hasTurret = false;
    float turretFacing = 0.0f;
    std::vector<std::int32_t> cargoManifest;
    std::int32_t embarkedOn = -1;
};

struct SavedWorld
{
    long iron = 0;
    long oil = 0;
    float ironCarry = 0.0f;
    float oilCarry = 0.0f;
    Camera2D camera = {};
    std::int32_t mapWidth = 0;
    std::int32_t mapHeight = 0;
    std::vector<std::uint8_t> terrain;
    std::vector<SavedUnit> units;
    std::vector<Building> buildings;
    std::vector<ResourceNode> nodes;
    float nodeIronCarry = 0.0f;
    float nodeOilCarry = 0.0f;
    std::vector<std::pair<int, std::vector<std::uint8_t>>> teamFog;
};

bool InRange(std::int64_t value)
{
    return value >= static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) &&
           value <= static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::max)());
}

bool DecodeUnit(const SaveUnit &in, SavedUnit &out, std::int32_t mapWidth,
                std::int32_t mapHeight)
{
    Unit &u = out.unit;
    if (in.armorType < 0 || in.armorType >= static_cast<int>(ArmorType::Count) ||
        in.damageType < 0 || in.damageType >= static_cast<int>(DamageType::Count) ||
        in.type < 0 || in.type >= static_cast<int>(UnitType::Count) || in.state < 0 ||
        in.state >= static_cast<int>(UnitState::Count) || in.phase < 0 ||
        in.phase >= static_cast<int>(AttackPhase::Count) || in.team < 0 ||
        in.team > kMaxTeamID)
    {
        return false;
    }
    if (static_cast<std::uint32_t>(in.path.size()) > kMaxPathNodes)
    {
        return false;
    }
    u.health = in.health;
    u.armorType = static_cast<ArmorType>(in.armorType);
    u.damageType = static_cast<DamageType>(in.damageType);
    u.attackPower = in.attackPower;
    u.attackRange = in.attackRange;
    CombatState &combat = out.combat;
    combat.cooldown = in.cooldown;
    u.cooldownTime = in.cooldownTime;
    combat.phase = static_cast<AttackPhase>(in.phase);
    combat.phaseTime = in.phaseTime;
    u.windupTime = in.windupTime;
    combat.lastDamageTaken = in.lastDamage;
    combat.hitFlashTime = in.hitFlash;
    u.speed = in.speed;
    u.sightRange = in.sightRange;
    u.position = ToVec2(in.position);
    u.velocity = ToVec2(in.velocity);
    u.isSelected = in.selected;
    u.teamID = in.team;
    u.type = static_cast<UnitType>(in.type);
    u.state = static_cast<UnitState>(in.state);
    out.targetIndex = in.targetIndex;
    Mover &mover = out.mover;
    mover.moveTarget = ToVec2(in.moveTarget);
    mover.hasMoveOrder = in.hasMoveOrder;
    mover.path.clear();
    mover.path.reserve(in.path.size());
    for (const SaveIVec2 &step : in.path)
    {
        if (step.x < 0 || step.x >= mapWidth || step.y < 0 || step.y >= mapHeight)
        {
            return false;
        }
        mover.path.push_back({ step.x, step.y });
    }
    if (in.pathNext > static_cast<std::uint32_t>(mover.path.size()))
    {
        return false;
    }
    mover.pathNext = static_cast<std::size_t>(in.pathNext);
    mover.hasPath = in.hasPath;
    combat.target = kInvalidEntity;
    if (in.facing >= 0 && in.facing < static_cast<std::int32_t>(Facing::Count))
    {
        u.facing = static_cast<Facing>(in.facing);
    }
    out.hasTurret = in.hasTurret;
    out.turretFacing = in.turretFacing;
    out.cargoManifest = in.cargoManifest;
    out.embarkedOn = in.embarkedOn;
    return true;
}

bool Decode(const std::string &payload, SavedWorld &out)
{
    SaveGameData msg;
    try
    {
        std::istringstream in(payload);
        cereal::JSONInputArchive ar(in);
        ar(msg);
    }
    catch (const std::exception &)
    {
        return false;
    }
    if (msg.saveVersion > kSaveVersion || msg.saveVersion < kMinSupportedVersion)
    {
        return false;
    }
    if (!InRange(msg.resources.iron) || !InRange(msg.resources.oil))
    {
        return false;
    }
    out.iron = static_cast<long>(msg.resources.iron);
    out.oil = static_cast<long>(msg.resources.oil);
    out.ironCarry = msg.resources.ironCarry;
    out.oilCarry = msg.resources.oilCarry;
    out.camera.target = ToVec2(msg.camera.target);
    out.camera.offset = ToVec2(msg.camera.offset);
    out.camera.rotation = msg.camera.rotation;
    out.camera.zoom = msg.camera.zoom;

    out.mapWidth = msg.map.width;
    out.mapHeight = msg.map.height;
    if (out.mapWidth <= 0 || out.mapHeight <= 0)
    {
        return false;
    }
    const std::int64_t tileCount = static_cast<std::int64_t>(out.mapWidth) * out.mapHeight;
    if (tileCount > kMaxMapTiles || msg.map.terrain.size() != static_cast<std::size_t>(tileCount))
    {
        return false;
    }
    out.terrain.resize(static_cast<std::size_t>(tileCount));
    for (std::int64_t i = 0; i < tileCount; ++i)
    {
        const std::int32_t t = msg.map.terrain[static_cast<std::size_t>(i)];
        if (t < 0 || t >= static_cast<std::int32_t>(TerrainType::Count))
        {
            return false;
        }
        out.terrain[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(t);
    }

    if (msg.units.size() > static_cast<std::size_t>(kMaxUnits))
    {
        return false;
    }
    out.units.clear();
    out.units.reserve(msg.units.size());
    for (const SaveUnit &in : msg.units)
    {
        SavedUnit saved;
        if (!DecodeUnit(in, saved, out.mapWidth, out.mapHeight))
        {
            return false;
        }
        out.units.push_back(saved);
    }

    if (msg.buildings.size() > static_cast<std::size_t>(kMaxBuildings))
    {
        return false;
    }
    out.buildings.clear();
    for (const SaveBuilding &in : msg.buildings)
    {
        if (in.type < 0 || in.type >= static_cast<int>(BuildingType::Count) || in.state < 0 ||
            in.state >= static_cast<int>(BuildingState::Count) || in.team < 0 ||
            in.team > kMaxTeamID)
        {
            return false;
        }
        Building b;
        b.type = static_cast<BuildingType>(in.type);
        b.state = static_cast<BuildingState>(in.state);
        const cc::IVec2 fp = Footprint(b.type);
        if (in.tileX < 0 || in.tileY < 0 || in.tileX > out.mapWidth - fp.x ||
            in.tileY > out.mapHeight - fp.y)
        {
            return false;
        }
        b.teamID = in.team;
        b.tileX = in.tileX;
        b.tileY = in.tileY;
        const float full = BuildingMaxHealth(b.type);
        b.maxHealth = in.maxHealth > 0.0f ? in.maxHealth : full;
        if (b.state == BuildingState::Destroyed)
        {
            b.health = 0.0f;
        }
        else if (b.state == BuildingState::UnderConstruction)
        {
            b.constructionTime = 0.0f;
            b.health = 0.0f;
        }
        else
        {
            b.health = in.health > 0.0f ? in.health : full;
        }
        if (b.health > b.maxHealth)
        {
            b.health = b.maxHealth;
        }
        out.buildings.push_back(b);
    }

    if (msg.nodes.size() > static_cast<std::size_t>(kMaxNodes))
    {
        return false;
    }
    out.nodes.clear();
    for (const SaveNode &in : msg.nodes)
    {
        if (in.kind < 0 || in.kind >= static_cast<int>(ResourceKind::Count))
        {
            return false;
        }
        if (in.tile.x < 0 || in.tile.x >= out.mapWidth || in.tile.y < 0 ||
            in.tile.y >= out.mapHeight)
        {
            return false;
        }
        ResourceNode node;
        node.kind = static_cast<ResourceKind>(in.kind);
        node.tile = { in.tile.x, in.tile.y };
        node.amount = in.amount;
        node.maxAmount = in.maxAmount;
        node.respawnDelay = in.respawnDelay;
        node.respawnTimer = in.respawnTimer;
        out.nodes.push_back(node);
    }
    out.nodeIronCarry = msg.nodeIronCarry;
    out.nodeOilCarry = msg.nodeOilCarry;
    if (msg.teamFog.size() > static_cast<std::uint32_t>(kMaxFogTeams))
    {
        return false;
    }
    out.teamFog.clear();
    for (const SaveFog &in : msg.teamFog)
    {
        out.teamFog.push_back({ in.team, in.explored });
    }
    return true;
}

}

bool SaveWorld(const WorldState &world, const std::string &path)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.camera == nullptr || world.nodes == nullptr || world.fog == nullptr)
    {
        return false;
    }
    SaveGameData msg;
    msg.saveVersion = kSaveVersion;
    msg.resources.iron = static_cast<std::int64_t>(world.resources->iron);
    msg.resources.oil = static_cast<std::int64_t>(world.resources->oil);
    msg.resources.ironCarry = world.resources->IronCarry();
    msg.resources.oilCarry = world.resources->OilCarry();
    FillVec2(msg.camera.target, world.camera->view.target);
    FillVec2(msg.camera.offset, world.camera->view.offset);
    msg.camera.rotation = world.camera->view.rotation;
    msg.camera.zoom = world.camera->view.zoom;
    msg.map.width = world.map->Width();
    msg.map.height = world.map->Height();
    for (int y = 0; y < world.map->Height(); ++y)
    {
        for (int x = 0; x < world.map->Width(); ++x)
        {
            msg.map.terrain.push_back(static_cast<std::int32_t>(world.map->Get({ x, y })));
        }
    }
    std::vector<Entity> order;
    world.registry->Each<Unit>([&](Entity id, const Unit &) { order.push_back(id); });
    for (Entity id : order)
    {
        const Unit &u = *world.registry->Get<Unit>(id);
        FillUnit(msg.units.emplace_back(), *world.registry, id, u, order);
    }
    world.registry->Each<Building>([&](Entity, const Building &b) {
        SaveBuilding &out = msg.buildings.emplace_back();
        out.type = static_cast<std::int32_t>(b.type);
        out.state = static_cast<std::int32_t>(b.state);
        out.team = b.teamID;
        out.tileX = b.tileX;
        out.tileY = b.tileY;
        out.health = b.health;
        out.maxHealth = b.maxHealth;
    });
    world.nodes->Each([&](const ResourceNode &node) {
        SaveNode &out = msg.nodes.emplace_back();
        out.kind = static_cast<std::int32_t>(node.kind);
        out.tile.x = node.tile.x;
        out.tile.y = node.tile.y;
        out.amount = node.amount;
        out.maxAmount = node.maxAmount;
        out.respawnDelay = node.respawnDelay;
        out.respawnTimer = node.respawnTimer;
    });
    msg.nodeIronCarry = world.nodes->IronCarry();
    msg.nodeOilCarry = world.nodes->OilCarry();
    for (int teamID : world.fog->Teams())
    {
        SaveFog &out = msg.teamFog.emplace_back();
        out.team = teamID;
        const std::vector<std::uint8_t> bytes = world.fog->ExploredBytes(teamID);
        out.explored.assign(bytes.begin(), bytes.end());
    }

    std::ostringstream payload(std::ios::binary);
    {
        cereal::JSONOutputArchive ar(payload);
        ar(msg);
    }
    const std::string bytes = payload.str();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return false;
    }
    file.write(kMagic, sizeof(kMagic));
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

bool LoadWorld(const WorldState &world, const std::string &path)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.camera == nullptr || world.nodes == nullptr || world.fog == nullptr)
    {
        return false;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= static_cast<std::streamsize>(sizeof(kMagic)))
    {
        return false;
    }
    file.seekg(0);
    char magic[sizeof(kMagic)] = {};
    if (!file.read(magic, sizeof(magic)) || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0)
    {
        return false;
    }
    std::string payload(static_cast<std::size_t>(size) - sizeof(kMagic), '\0');
    if (!file.read(payload.data(), static_cast<std::streamsize>(payload.size())))
    {
        return false;
    }
    SavedWorld saved;
    if (!Decode(payload, saved))
    {
        return false;
    }

    world.registry->Clear();
    world.resources->iron = saved.iron;
    world.resources->oil = saved.oil;
    world.resources->SetCarry(saved.ironCarry, saved.oilCarry);
    world.camera->view = saved.camera;
    world.map->Resize(saved.mapWidth, saved.mapHeight);
    if (world.occ != nullptr)
    {
        world.occ->Clear();
        world.occ->Resize(saved.mapWidth, saved.mapHeight);
    }
    for (int y = 0; y < saved.mapHeight; ++y)
    {
        for (int x = 0; x < saved.mapWidth; ++x)
        {
            world.map->Set({ x, y },
                           static_cast<TerrainType>(
                               saved.terrain[static_cast<std::size_t>(y) * static_cast<std::size_t>(saved.mapWidth) +
                                             static_cast<std::size_t>(x)]));
        }
    }
    std::vector<Entity> freshIds;
    freshIds.reserve(saved.units.size());
    for (const SavedUnit &savedUnit : saved.units)
    {
        freshIds.push_back(world.registry->Create());
        world.registry->Add(freshIds.back(), savedUnit.unit);
        world.registry->Add(freshIds.back(), Orders{});
        world.registry->Add(freshIds.back(), savedUnit.mover);
        world.registry->Add(freshIds.back(), savedUnit.combat);
    }
    for (std::size_t i = 0; i < saved.units.size(); ++i)
    {
        const std::int32_t targetIndex = saved.units[i].targetIndex;
        if (targetIndex >= 0 && static_cast<std::size_t>(targetIndex) < freshIds.size())
        {
            GetCombatState(*world.registry, freshIds[i]).target =
                freshIds[static_cast<std::size_t>(targetIndex)];
        }
        // M2 extension restore: turret facing + cargo manifest + embarked
        // marker (same remap pattern as targetIndex; tolerant wire defaults
        // mean old saves simply get no components).
        const SavedUnit &savedUnit = saved.units[i];
        if (savedUnit.hasTurret)
        {
            Turret turret;
            turret.facing = savedUnit.turretFacing;
            turret.turnRate =
                ActiveUnitConfig(savedUnit.unit.type).abilities.turretTurnRate;
            world.registry->Add(freshIds[i], turret);
        }
        if (!savedUnit.cargoManifest.empty())
        {
            Cargo cargo;
            cargo.capacity =
                ActiveUnitConfig(savedUnit.unit.type).abilities.transportCapacity;
            for (const std::int32_t passengerIndex : savedUnit.cargoManifest)
            {
                if (passengerIndex >= 0 &&
                    static_cast<std::size_t>(passengerIndex) < freshIds.size())
                {
                    cargo.passengers.push_back(freshIds[static_cast<std::size_t>(passengerIndex)]);
                }
            }
            world.registry->Add(freshIds[i], cargo);
        }
        if (savedUnit.embarkedOn >= 0 &&
            static_cast<std::size_t>(savedUnit.embarkedOn) < freshIds.size())
        {
            EmbarkedOn ride;
            ride.carrier = freshIds[static_cast<std::size_t>(savedUnit.embarkedOn)];
            world.registry->Add(freshIds[i], ride);
        }
    }
    for (const Building &b : saved.buildings)
    {
        world.registry->Add(world.registry->Create(), b);
    }
    *world.nodes = ResourceNodes();
    for (const ResourceNode &node : saved.nodes)
    {
        world.nodes->RestoreNode(node.kind, node.tile, node.amount, node.maxAmount, node.respawnDelay,
                                 node.respawnTimer);
    }
    world.nodes->SetCarry(saved.nodeIronCarry, saved.nodeOilCarry);
    world.fog->Resize(saved.mapWidth, saved.mapHeight);
    for (const auto &entry : saved.teamFog)
    {
        const std::vector<std::uint8_t> &bytes = entry.second;
        world.fog->SetExplored(entry.first, bytes.data(), bytes.size());
    }
    return true;
}

std::string SaveSlotPath(int slot)
{
    if (slot < 1)
    {
        slot = 1;
    }
    if (slot > 3)
    {
        slot = 3;
    }
    return "data/slot" + std::to_string(slot) + ".ccpb";
}

std::string ReplayFramePath(const std::string &dir, int index)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "replay_%04d.ccpb", index < 0 ? 0 : index);
    return dir + "/" + buf;
}

int ReplayFrameCount(const std::string &dir)
{
    int count = 0;
    std::error_code ec;
    while (count < kReplayMaxFrames)
    {
        if (!std::filesystem::exists(ReplayFramePath(dir, count), ec) || ec)
        {
            break;
        }
        ++count;
    }
    return count;
}
