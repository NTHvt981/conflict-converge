#include "SaveGame.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "Building.h"
#include "CcAssert.h"
#include "GameCamera.h"
#include "Nodes.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "TileMap.h"
#include "Unit.h"

namespace
{

constexpr char kMagic[4] = { 'C', 'C', 'S', 'V' };
// Garbage-input guards: saves violating these are rejected, never trusted.
constexpr std::int32_t kMaxMapTiles = 1024 * 1024;
constexpr std::int32_t kMaxUnits = 100000;
constexpr std::int32_t kMaxBuildings = 100000;
constexpr std::int32_t kMaxNodes = 100000;
constexpr std::uint32_t kMaxPathNodes = 100000;

class Writer
{
public:
    template <typename T> void Write(T value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const std::uint8_t *bytes = reinterpret_cast<const std::uint8_t *>(&value);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(T));
    }
    void WriteBool(bool value)
    {
        buffer_.push_back(value ? 1 : 0);
    }
    const std::vector<std::uint8_t> &Buffer() const
    {
        return buffer_;
    }

private:
    std::vector<std::uint8_t> buffer_;
};

class Reader
{
public:
    Reader(const std::uint8_t *data, std::size_t size) : data_(data), size_(size)
    {
    }
    template <typename T> bool Read(T &out)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (pos_ + sizeof(T) > size_)
        {
            return false;
        }
        std::memcpy(&out, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return true;
    }
    bool ReadBool(bool &out)
    {
        std::uint8_t byte = 0;
        if (!Read(byte))
        {
            return false;
        }
        out = byte != 0;
        return true;
    }
    bool AtEnd() const
    {
        return pos_ == size_;
    }

private:
    const std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
};

void WriteVec2(Writer &out, Vector2 v)
{
    out.Write(v.x);
    out.Write(v.y);
}

bool ReadVec2(Reader &in, Vector2 &v)
{
    return in.Read(v.x) && in.Read(v.y);
}

// Decoded snapshot: LoadWorld parses the whole buffer into these first and
// only touches the live world when every byte checks out.
struct SavedUnit
{
    Unit unit;
    std::int32_t targetIndex = -1;
};

struct SavedWorld
{
    std::int32_t iron = 0;
    std::int32_t oil = 0;
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
};

bool Decode(const std::vector<std::uint8_t> &bytes, SavedWorld &out)
{
    Reader in(bytes.data(), bytes.size());
    char magic[4] = {};
    for (char &c : magic)
    {
        if (!in.Read(c))
        {
            return false;
        }
    }
    if (std::memcmp(magic, kMagic, sizeof(kMagic)) != 0)
    {
        return false;
    }
    std::uint32_t version = 0;
    if (!in.Read(version) || version != kSaveVersion)
    {
        return false;
    }
    if (!in.Read(out.iron) || !in.Read(out.oil) || !in.Read(out.ironCarry) || !in.Read(out.oilCarry))
    {
        return false;
    }
    if (!ReadVec2(in, out.camera.target) || !ReadVec2(in, out.camera.offset) || !in.Read(out.camera.rotation) ||
        !in.Read(out.camera.zoom))
    {
        return false;
    }
    if (!in.Read(out.mapWidth) || !in.Read(out.mapHeight) || out.mapWidth <= 0 || out.mapHeight <= 0)
    {
        return false;
    }
    const std::int64_t tileCount = static_cast<std::int64_t>(out.mapWidth) * out.mapHeight;
    if (tileCount > kMaxMapTiles)
    {
        return false;
    }
    out.terrain.resize(static_cast<std::size_t>(tileCount));
    for (std::uint8_t &t : out.terrain)
    {
        if (!in.Read(t) || t > static_cast<std::uint8_t>(TerrainType::Building))
        {
            return false;
        }
    }
    std::int32_t unitCount = 0;
    if (!in.Read(unitCount) || unitCount < 0 || unitCount > kMaxUnits)
    {
        return false;
    }
    out.units.resize(static_cast<std::size_t>(unitCount));
    for (SavedUnit &saved : out.units)
    {
        Unit &u = saved.unit;
        std::int32_t type = 0, armor = 0, damage = 0, state = 0, phase = 0;
        if (!in.Read(u.health) || !in.Read(armor) || !in.Read(damage) || !in.Read(u.attackPower) ||
            !in.Read(u.attackRange) || !in.Read(u.cooldown) || !in.Read(u.cooldownTime) || !in.Read(phase) ||
            !in.Read(u.phaseTime) || !in.Read(u.windupTime) || !in.Read(u.lastDamageTaken) ||
            !in.Read(u.hitFlashTime) || !in.Read(u.speed) || !in.Read(u.sightRange) || !ReadVec2(in, u.position) ||
            !ReadVec2(in, u.velocity) || !in.ReadBool(u.isSelected) || !in.Read(u.teamID) || !in.Read(type) ||
            !in.Read(state) || !in.Read(saved.targetIndex) || !ReadVec2(in, u.moveTarget) ||
            !in.ReadBool(u.hasMoveOrder))
        {
            return false;
        }
        if (type < 0 || type > static_cast<std::int32_t>(UnitType::HeavyTank) || state < 0 ||
            state > static_cast<std::int32_t>(UnitState::Attacking) || phase < 0 ||
            phase > static_cast<std::int32_t>(AttackPhase::Recover))
        {
            return false;
        }
        u.armorType = static_cast<ArmorType>(armor);
        u.damageType = static_cast<DamageType>(damage);
        u.type = static_cast<UnitType>(type);
        u.state = static_cast<UnitState>(state);
        u.phase = static_cast<AttackPhase>(phase);
        if (armor < 0 || armor > static_cast<std::int32_t>(ArmorType::COMPOSITE) || damage < 0 ||
            damage > static_cast<std::int32_t>(DamageType::ENERGY))
        {
            return false;
        }
        std::uint32_t pathCount = 0;
        if (!in.Read(pathCount) || pathCount > kMaxPathNodes)
        {
            return false;
        }
        u.path.resize(pathCount);
        for (cc::IVec2 &step : u.path)
        {
            if (!in.Read(step.x) || !in.Read(step.y))
            {
                return false;
            }
        }
        std::uint32_t pathNext = 0;
        if (!in.Read(pathNext) || !in.ReadBool(u.hasPath))
        {
            return false;
        }
        u.pathNext = pathNext;
        u.target = kInvalidEntity; // remapped to fresh IDs at commit time
    }
    std::int32_t buildingCount = 0;
    if (!in.Read(buildingCount) || buildingCount < 0 || buildingCount > kMaxBuildings)
    {
        return false;
    }
    out.buildings.resize(static_cast<std::size_t>(buildingCount));
    for (Building &b : out.buildings)
    {
        std::int32_t type = 0, state = 0;
        if (!in.Read(type) || !in.Read(state) || !in.Read(b.teamID) || !in.Read(b.tileX) || !in.Read(b.tileY))
        {
            return false;
        }
        if (type < 0 || type > static_cast<std::int32_t>(BuildingType::Factory) || state < 0 ||
            state > static_cast<std::int32_t>(BuildingState::Destroyed))
        {
            return false;
        }
        b.type = static_cast<BuildingType>(type);
        b.state = static_cast<BuildingState>(state);
    }
    std::int32_t nodeCount = 0;
    if (!in.Read(nodeCount) || nodeCount < 0 || nodeCount > kMaxNodes)
    {
        return false;
    }
    out.nodes.resize(static_cast<std::size_t>(nodeCount));
    for (ResourceNode &node : out.nodes)
    {
        std::int32_t kind = 0;
        if (!in.Read(kind) || !in.Read(node.tile.x) || !in.Read(node.tile.y) || !in.Read(node.amount) ||
            !in.Read(node.maxAmount) || !in.Read(node.respawnDelay) || !in.Read(node.respawnTimer))
        {
            return false;
        }
        if (kind < 0 || kind > static_cast<std::int32_t>(ResourceKind::Oil))
        {
            return false;
        }
        node.kind = static_cast<ResourceKind>(kind);
    }
    return in.Read(out.nodeIronCarry) && in.Read(out.nodeOilCarry) && in.AtEnd();
}

} // namespace

bool SaveWorld(const WorldState &world, const std::string &path)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.camera == nullptr || world.nodes == nullptr)
    {
        return false;
    }
    Writer out;
    for (char c : kMagic)
    {
        out.Write(c);
    }
    out.Write(kSaveVersion);
    out.Write(static_cast<std::int32_t>(world.resources->iron));
    out.Write(static_cast<std::int32_t>(world.resources->oil));
    out.Write(world.resources->IronCarry());
    out.Write(world.resources->OilCarry());
    WriteVec2(out, world.camera->view.target);
    WriteVec2(out, world.camera->view.offset);
    out.Write(world.camera->view.rotation);
    out.Write(world.camera->view.zoom);
    out.Write(world.map->Width());
    out.Write(world.map->Height());
    for (int y = 0; y < world.map->Height(); ++y)
    {
        for (int x = 0; x < world.map->Width(); ++x)
        {
            out.Write(static_cast<std::uint8_t>(world.map->Get({ x, y })));
        }
    }
    // Units in Each() order; targets stored as indices into that same order.
    std::vector<Entity> order;
    world.registry->Each<Unit>([&](Entity id, const Unit &) { order.push_back(id); });
    out.Write(static_cast<std::int32_t>(order.size()));
    for (Entity id : order)
    {
        const Unit &u = *world.registry->Get<Unit>(id);
        out.Write(u.health);
        out.Write(static_cast<std::int32_t>(u.armorType));
        out.Write(static_cast<std::int32_t>(u.damageType));
        out.Write(u.attackPower);
        out.Write(u.attackRange);
        out.Write(u.cooldown);
        out.Write(u.cooldownTime);
        out.Write(static_cast<std::int32_t>(u.phase));
        out.Write(u.phaseTime);
        out.Write(u.windupTime);
        out.Write(u.lastDamageTaken);
        out.Write(u.hitFlashTime);
        out.Write(u.speed);
        out.Write(u.sightRange);
        WriteVec2(out, u.position);
        WriteVec2(out, u.velocity);
        out.WriteBool(u.isSelected);
        out.Write(u.teamID);
        out.Write(static_cast<std::int32_t>(u.type));
        out.Write(static_cast<std::int32_t>(u.state));
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
        out.Write(targetIndex);
        WriteVec2(out, u.moveTarget);
        out.WriteBool(u.hasMoveOrder);
        out.Write(static_cast<std::uint32_t>(u.path.size()));
        for (const cc::IVec2 &step : u.path)
        {
            out.Write(step.x);
            out.Write(step.y);
        }
        out.Write(static_cast<std::uint32_t>(u.pathNext));
        out.WriteBool(u.hasPath);
    }
    std::vector<Building> buildings;
    world.registry->Each<Building>([&](Entity, const Building &b) { buildings.push_back(b); });
    out.Write(static_cast<std::int32_t>(buildings.size()));
    for (const Building &b : buildings)
    {
        out.Write(static_cast<std::int32_t>(b.type));
        out.Write(static_cast<std::int32_t>(b.state));
        out.Write(b.teamID);
        out.Write(b.tileX);
        out.Write(b.tileY);
    }
    std::vector<ResourceNode> nodes;
    world.nodes->Each([&](const ResourceNode &node) { nodes.push_back(node); });
    out.Write(static_cast<std::int32_t>(nodes.size()));
    for (const ResourceNode &node : nodes)
    {
        out.Write(static_cast<std::int32_t>(node.kind));
        out.Write(node.tile.x);
        out.Write(node.tile.y);
        out.Write(node.amount);
        out.Write(node.maxAmount);
        out.Write(node.respawnDelay);
        out.Write(node.respawnTimer);
    }
    out.Write(world.nodes->IronCarry());
    out.Write(world.nodes->OilCarry());

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return false;
    }
    const std::vector<std::uint8_t> &buffer = out.Buffer();
    file.write(reinterpret_cast<const char *>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));
    return static_cast<bool>(file);
}

bool LoadWorld(const WorldState &world, const std::string &path)
{
    if (world.registry == nullptr || world.resources == nullptr || world.map == nullptr ||
        world.camera == nullptr || world.nodes == nullptr)
    {
        return false;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0)
    {
        return false;
    }
    file.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char *>(bytes.data()), size))
    {
        return false;
    }
    SavedWorld saved;
    if (!Decode(bytes, saved))
    {
        return false; // destination world untouched
    }

    // Commit: every byte validated, apply in dependency order.
    world.registry->Clear();
    world.resources->iron = saved.iron;
    world.resources->oil = saved.oil;
    world.resources->SetCarry(saved.ironCarry, saved.oilCarry);
    world.camera->view = saved.camera;
    world.map->Resize(saved.mapWidth, saved.mapHeight);
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
    return true;
}
