#include "SaveGame.h"

#include <cstdint>
#include <cstdio> // snprintf for replay frame names
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "savegame.pb.h"

#include "Building.h"
#include "CcAssert.h"
#include "FogOfWar.h" // M9: per-team explored sets via team_fog
#include "GameCamera.h"
#include "Nodes.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "TileMap.h"
#include "Unit.h"

namespace
{

constexpr char kMagic[4] = { 'C', 'C', 'P', 'B' };
// Garbage-input guards: saves violating these are rejected, never trusted.
constexpr std::int32_t kMaxMapTiles = 1024 * 1024;
constexpr std::int32_t kMaxUnits = 100000;
constexpr std::int32_t kMaxBuildings = 100000;
constexpr std::int32_t kMaxNodes = 100000;
constexpr std::uint32_t kMaxPathNodes = 100000;
constexpr std::int32_t kMaxFogTeams = 16;
// Playable sides: teams are 0 (player/allies) and 1 (enemies) in every mode
// (2v2 shares teams, it never adds new ones). Anything else on the wire is
// corruption, never a future extension — reject it.
constexpr int kMaxTeamID = 1;

// Wire-range pins: every enum validated below ends in a Count sentinel, and
// the decode checks accept [0, Count). Appending a variant bumps Count and
// trips these asserts on purpose — extend the decode handling deliberately
// (defaults? migration?), never silently.
static_assert(static_cast<int>(ArmorType::Count) == 3, "ArmorType grew: review save decode");
static_assert(static_cast<int>(DamageType::Count) == 3, "DamageType grew: review save decode");
static_assert(static_cast<int>(UnitType::Count) == 8, "UnitType grew: review save decode");
static_assert(static_cast<int>(UnitState::Count) == 3, "UnitState grew: review save decode");
static_assert(static_cast<int>(AttackPhase::Count) == 3, "AttackPhase grew: review save decode");
static_assert(static_cast<int>(TerrainType::Count) == 5, "TerrainType grew: review save decode");
static_assert(static_cast<int>(BuildingType::Count) == 3, "BuildingType grew: review save decode");
static_assert(static_cast<int>(BuildingState::Count) == 3, "BuildingState grew: review save decode");
// Reviewed: UnderConstruction appended AFTER Destroyed, so wire values 0/1
// decode exactly like legacy saves; 2 loads as UnderConstruction with a
// restarted timer (constructionTime is not serialized — see below).
static_assert(static_cast<int>(ResourceKind::Count) == 2, "ResourceKind grew: review save decode");

void FillVec2(cc::save::Vec2 *out, Vector2 v)
{
    out->set_x(v.x);
    out->set_y(v.y);
}

Vector2 ToVec2(const cc::save::Vec2 &in)
{
    return { in.x(), in.y() };
}

void FillUnit(cc::save::Unit *out, const Unit &u, std::int32_t targetIndex)
{
    out->set_health(u.health);
    out->set_armor_type(static_cast<std::int32_t>(u.armorType));
    out->set_damage_type(static_cast<std::int32_t>(u.damageType));
    out->set_attack_power(u.attackPower);
    out->set_attack_range(u.attackRange);
    out->set_cooldown(u.cooldown);
    out->set_cooldown_time(u.cooldownTime);
    out->set_phase(static_cast<std::int32_t>(u.phase));
    out->set_phase_time(u.phaseTime);
    out->set_windup_time(u.windupTime);
    out->set_last_damage(u.lastDamageTaken);
    out->set_hit_flash(u.hitFlashTime);
    out->set_speed(u.speed);
    out->set_sight_range(u.sightRange);
    FillVec2(out->mutable_position(), u.position);
    FillVec2(out->mutable_velocity(), u.velocity);
    out->set_selected(u.isSelected);
    out->set_team(u.teamID);
    out->set_type(static_cast<std::int32_t>(u.type));
    out->set_state(static_cast<std::int32_t>(u.state));
    out->set_target_index(targetIndex);
    FillVec2(out->mutable_move_target(), u.moveTarget);
    out->set_has_move_order(u.hasMoveOrder);
    for (const cc::IVec2 &step : u.path)
    {
        cc::save::IVec2 *dst = out->add_path();
        dst->set_x(step.x);
        dst->set_y(step.y);
    }
    out->set_path_next(static_cast<std::uint32_t>(u.pathNext));
    out->set_has_path(u.hasPath);
}

// Decoded snapshot: LoadWorld parses the whole message into these first and
// only touches the live world when every field checks out.
struct SavedUnit
{
    Unit unit;
    std::int32_t targetIndex = -1;
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
    std::vector<std::pair<int, std::string>> teamFog; // (teamID, explored bytes)
};

bool InRange(std::int64_t value)
{
    return value >= static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) &&
           value <= static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::max)());
}

bool DecodeUnit(const cc::save::Unit &in, SavedUnit &out, std::int32_t mapWidth,
                std::int32_t mapHeight)
{
    Unit &u = out.unit;
    if (in.armor_type() < 0 || in.armor_type() >= static_cast<int>(ArmorType::Count) ||
        in.damage_type() < 0 || in.damage_type() >= static_cast<int>(DamageType::Count) ||
        in.type() < 0 || in.type() >= static_cast<int>(UnitType::Count) || in.state() < 0 ||
        in.state() >= static_cast<int>(UnitState::Count) || in.phase() < 0 ||
        in.phase() >= static_cast<int>(AttackPhase::Count) || in.team() < 0 ||
        in.team() > kMaxTeamID)
    {
        return false;
    }
    if (static_cast<std::uint32_t>(in.path_size()) > kMaxPathNodes)
    {
        return false;
    }
    u.health = in.health();
    u.armorType = static_cast<ArmorType>(in.armor_type());
    u.damageType = static_cast<DamageType>(in.damage_type());
    u.attackPower = in.attack_power();
    u.attackRange = in.attack_range();
    u.cooldown = in.cooldown();
    u.cooldownTime = in.cooldown_time();
    u.phase = static_cast<AttackPhase>(in.phase());
    u.phaseTime = in.phase_time();
    u.windupTime = in.windup_time();
    u.lastDamageTaken = in.last_damage();
    u.hitFlashTime = in.hit_flash();
    u.speed = in.speed();
    u.sightRange = in.sight_range();
    u.position = ToVec2(in.position());
    u.velocity = ToVec2(in.velocity());
    u.isSelected = in.selected();
    u.teamID = in.team();
    u.type = static_cast<UnitType>(in.type());
    u.state = static_cast<UnitState>(in.state());
    out.targetIndex = in.target_index();
    u.moveTarget = ToVec2(in.move_target());
    u.hasMoveOrder = in.has_move_order();
    u.path.clear();
    u.path.reserve(static_cast<std::size_t>(in.path_size()));
    for (const cc::save::IVec2 &step : in.path())
    {
        if (step.x() < 0 || step.x() >= mapWidth || step.y() < 0 || step.y() >= mapHeight)
        {
            return false;
        }
        u.path.push_back({ step.x(), step.y() });
    }
    if (in.path_next() > static_cast<std::uint32_t>(u.path.size()))
    {
        return false;
    }
    u.pathNext = static_cast<std::size_t>(in.path_next());
    u.hasPath = in.has_path();
    u.target = kInvalidEntity; // remapped to fresh IDs at commit time
    return true;
}

bool Decode(const std::string &payload, SavedWorld &out)
{
    cc::save::SaveGame msg;
    if (!msg.ParseFromString(payload))
    {
        return false;
    }
    if (msg.save_version() != kSaveVersion)
    {
        return false;
    }
    if (!InRange(msg.resources().iron()) || !InRange(msg.resources().oil()))
    {
        return false;
    }
    out.iron = static_cast<long>(msg.resources().iron());
    out.oil = static_cast<long>(msg.resources().oil());
    out.ironCarry = msg.resources().iron_carry();
    out.oilCarry = msg.resources().oil_carry();
    out.camera.target = ToVec2(msg.camera().target());
    out.camera.offset = ToVec2(msg.camera().offset());
    out.camera.rotation = msg.camera().rotation();
    out.camera.zoom = msg.camera().zoom();

    out.mapWidth = msg.map().width();
    out.mapHeight = msg.map().height();
    if (out.mapWidth <= 0 || out.mapHeight <= 0)
    {
        return false;
    }
    const std::int64_t tileCount = static_cast<std::int64_t>(out.mapWidth) * out.mapHeight;
    if (tileCount > kMaxMapTiles || msg.map().terrain_size() != tileCount)
    {
        return false;
    }
    out.terrain.resize(static_cast<std::size_t>(tileCount));
    for (std::int64_t i = 0; i < tileCount; ++i)
    {
        const std::int32_t t = msg.map().terrain(static_cast<int>(i));
        if (t < 0 || t >= static_cast<std::int32_t>(TerrainType::Count))
        {
            return false;
        }
        out.terrain[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(t);
    }

    if (msg.units_size() > kMaxUnits)
    {
        return false;
    }
    out.units.clear();
    out.units.reserve(static_cast<std::size_t>(msg.units_size()));
    for (const cc::save::Unit &in : msg.units())
    {
        SavedUnit saved;
        if (!DecodeUnit(in, saved, out.mapWidth, out.mapHeight))
        {
            return false;
        }
        out.units.push_back(saved);
    }

    if (msg.buildings_size() > kMaxBuildings)
    {
        return false;
    }
    out.buildings.clear();
    for (const cc::save::Building &in : msg.buildings())
    {
        if (in.type() < 0 || in.type() >= static_cast<int>(BuildingType::Count) || in.state() < 0 ||
            in.state() >= static_cast<int>(BuildingState::Count) || in.team() < 0 ||
            in.team() > kMaxTeamID)
        {
            return false;
        }
        Building b;
        b.type = static_cast<BuildingType>(in.type());
        b.state = static_cast<BuildingState>(in.state());
        // The whole footprint must sit inside the save's own map, not just
        // the anchor — downstream code indexes every footprint tile.
        const cc::IVec2 fp = Footprint(b.type);
        if (in.tile_x() < 0 || in.tile_y() < 0 || in.tile_x() + fp.x > out.mapWidth ||
            in.tile_y() + fp.y > out.mapHeight)
        {
            return false;
        }
        b.teamID = in.team();
        b.tileX = in.tile_x();
        b.tileY = in.tile_y();
        // M13: legacy saves predate building HP (zeros on the wire) — heal
        // those to full rather than loading rubble. Destroyed buildings
        // legitimately save health=0, so the heal only applies while the
        // structure is still standing.
        const float full = BuildingMaxHealth(b.type);
        b.maxHealth = in.max_health() > 0.0f ? in.max_health() : full;
        if (b.state == BuildingState::Destroyed)
        {
            b.health = 0.0f;
        }
        else if (b.state == BuildingState::UnderConstruction)
        {
            // Timer isn't serialized: restart construction from 0 rather
            // than resuming mid-ramp (also dodges the legacy zero-heal
            // above, which would load a fresh site at full health).
            b.constructionTime = 0.0f;
            b.health = 0.0f;
        }
        else
        {
            b.health = in.health() > 0.0f ? in.health() : full;
        }
        if (b.health > b.maxHealth)
        {
            b.health = b.maxHealth;
        }
        out.buildings.push_back(b);
    }

    if (msg.nodes_size() > kMaxNodes)
    {
        return false;
    }
    out.nodes.clear();
    for (const cc::save::ResourceNode &in : msg.nodes())
    {
        if (in.kind() < 0 || in.kind() >= static_cast<int>(ResourceKind::Count))
        {
            return false;
        }
        if (in.tile().x() < 0 || in.tile().x() >= out.mapWidth || in.tile().y() < 0 ||
            in.tile().y() >= out.mapHeight)
        {
            return false;
        }
        ResourceNode node;
        node.kind = static_cast<ResourceKind>(in.kind());
        node.tile = { in.tile().x(), in.tile().y() };
        node.amount = in.amount();
        node.maxAmount = in.max_amount();
        node.respawnDelay = in.respawn_delay();
        node.respawnTimer = in.respawn_timer();
        out.nodes.push_back(node);
    }
    out.nodeIronCarry = msg.node_iron_carry();
    out.nodeOilCarry = msg.node_oil_carry();
    // msg.explored() is the legacy M15 field: superseded, never read.
    if (msg.team_fog_size() > kMaxFogTeams)
    {
        return false;
    }
    out.teamFog.clear();
    for (const cc::save::TeamFog &in : msg.team_fog())
    {
        out.teamFog.push_back({ in.team(), in.explored() });
    }
    return true;
}

} // namespace

bool SaveWorld(const WorldState &world, const std::string &path)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.camera == nullptr || world.nodes == nullptr || world.fog == nullptr)
    {
        return false;
    }
    cc::save::SaveGame msg;
    msg.set_save_version(kSaveVersion);
    msg.mutable_resources()->set_iron(static_cast<std::int64_t>(world.resources->iron));
    msg.mutable_resources()->set_oil(static_cast<std::int64_t>(world.resources->oil));
    msg.mutable_resources()->set_iron_carry(world.resources->IronCarry());
    msg.mutable_resources()->set_oil_carry(world.resources->OilCarry());
    FillVec2(msg.mutable_camera()->mutable_target(), world.camera->view.target);
    FillVec2(msg.mutable_camera()->mutable_offset(), world.camera->view.offset);
    msg.mutable_camera()->set_rotation(world.camera->view.rotation);
    msg.mutable_camera()->set_zoom(world.camera->view.zoom);
    msg.mutable_map()->set_width(world.map->Width());
    msg.mutable_map()->set_height(world.map->Height());
    for (int y = 0; y < world.map->Height(); ++y)
    {
        for (int x = 0; x < world.map->Width(); ++x)
        {
            msg.mutable_map()->add_terrain(static_cast<std::int32_t>(world.map->Get({ x, y })));
        }
    }
    // Units in Each() order; targets stored as indices into that same order.
    std::vector<Entity> order;
    world.registry->Each<Unit>([&](Entity id, const Unit &) { order.push_back(id); });
    for (Entity id : order)
    {
        const Unit &u = *world.registry->Get<Unit>(id);
        std::int32_t targetIndex = -1;
        if (u.target != kInvalidEntity)
        {
            for (std::size_t i = 0; i < order.size(); ++i)
            {
                if (order[i] == u.target)
                {
                    targetIndex = static_cast<std::int32_t>(i);
                    break;
                }
            }
        }
        FillUnit(msg.add_units(), u, targetIndex);
    }
    world.registry->Each<Building>([&](Entity, const Building &b) {
        cc::save::Building *out = msg.add_buildings();
        out->set_type(static_cast<std::int32_t>(b.type));
        out->set_state(static_cast<std::int32_t>(b.state));
        out->set_team(b.teamID);
        out->set_tile_x(b.tileX);
        out->set_tile_y(b.tileY);
        out->set_health(b.health);
        out->set_max_health(b.maxHealth);
    });
    world.nodes->Each([&](const ResourceNode &node) {
        cc::save::ResourceNode *out = msg.add_nodes();
        out->set_kind(static_cast<std::int32_t>(node.kind));
        out->mutable_tile()->set_x(node.tile.x);
        out->mutable_tile()->set_y(node.tile.y);
        out->set_amount(node.amount);
        out->set_max_amount(node.maxAmount);
        out->set_respawn_delay(node.respawnDelay);
        out->set_respawn_timer(node.respawnTimer);
    });
    msg.set_node_iron_carry(world.nodes->IronCarry());
    msg.set_node_oil_carry(world.nodes->OilCarry());
    for (int teamID : world.fog->Teams())
    {
        cc::save::TeamFog *out = msg.add_team_fog();
        out->set_team(teamID);
        const std::vector<std::uint8_t> bytes = world.fog->ExploredBytes(teamID);
        out->set_explored(bytes.data(), bytes.size());
    }

    std::string payload;
    if (!msg.SerializeToString(&payload))
    {
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return false;
    }
    file.write(kMagic, sizeof(kMagic));
    file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
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
        return false; // rejects old CCSV files and garbage alike
    }
    std::string payload(static_cast<std::size_t>(size) - sizeof(kMagic), '\0');
    if (!file.read(payload.data(), static_cast<std::streamsize>(payload.size())))
    {
        return false;
    }
    SavedWorld saved;
    if (!Decode(payload, saved))
    {
        return false; // destination world untouched
    }

    // Commit: every field validated, apply in dependency order.
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
    }
    for (std::size_t i = 0; i < saved.units.size(); ++i)
    {
        const std::int32_t targetIndex = saved.units[i].targetIndex;
        if (targetIndex >= 0 && static_cast<std::size_t>(targetIndex) < freshIds.size())
        {
            world.registry->Get<Unit>(freshIds[i])->target = freshIds[static_cast<std::size_t>(targetIndex)];
        }
    }
    for (const Building &b : saved.buildings)
    {
        world.registry->Add(world.registry->Create(), b); // tiles already marked by map data
    }
    // Fresh node storage: same default state a new ResourceNodes starts in.
    *world.nodes = ResourceNodes();
    for (const ResourceNode &node : saved.nodes)
    {
        world.nodes->RestoreNode(node.kind, node.tile, node.amount, node.maxAmount, node.respawnDelay,
                                 node.respawnTimer);
    }
    world.nodes->SetCarry(saved.nodeIronCarry, saved.nodeOilCarry);
    // Fog memory follows the map: resize first, then restore each team's
    // explored set (size mismatches are ignored inside SetExplored).
    world.fog->Resize(saved.mapWidth, saved.mapHeight);
    for (const auto &entry : saved.teamFog)
    {
        world.fog->SetExplored(entry.first,
                               reinterpret_cast<const std::uint8_t *>(entry.second.data()),
                               entry.second.size());
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
