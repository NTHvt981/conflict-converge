
#include "app/save/SaveGame.h"

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app/save/SaveWire.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "world/FogOfWar.h"
#include "app/ui/GameCamera.h"
#include "economy/Nodes.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "world/TileMap.h"
#include "units/Unit.h"

namespace
{

void FillVec2(SaveVec2 &out, Vector2 v)
{
    out.x = v.x;
    out.y = v.y;
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
