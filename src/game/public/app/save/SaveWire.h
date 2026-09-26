#pragma once

// Tolerant JSON wire format ("CCJ3" magic, saveVersion 3+).
//
// Contract: NVP NAMES are the on-disk contract (never rename one).
// The payload root is {"value0": {...}}: cereal auto-names the unnamed
// top-level struct, so hand-written saves must wrap in "value0" too.
// Additive changes only: append new fields with defaults; never remove or
// reinterpret a field. Every field except saveVersion loads tolerantly
// (missing -> the member-initializer default), so old saves keep loading as
// the unit surface grows. Unknown enum values still fail validation in
// Decode (SaveGame.cpp) rather than here, and versions outside
// [kMinSupportedVersion, kSaveVersion] are rejected outright.

#include <cereal/cereal.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace save_detail
{

// Load one NVP without failing when it is absent (mirrors UnitConfig's
// TryLoadValue); a present-but-malformed value still throws via the archive.
template <class Archive, class T>
void TryLoadValue(Archive &ar, const char *name, T &out)
{
    try
    {
        ar(cereal::make_nvp(name, out));
    }
    catch (const cereal::Exception &)
    {
    }
}

} // namespace save_detail

struct SaveVec2
{
    float x = 0.0f;
    float y = 0.0f;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("x", x), cereal::make_nvp("y", y));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "x", x);
        save_detail::TryLoadValue(ar, "y", y);
    }
};

struct SaveIVec2
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("x", x), cereal::make_nvp("y", y));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "x", x);
        save_detail::TryLoadValue(ar, "y", y);
    }
};

struct SaveResources
{
    std::int64_t iron = 0;
    std::int64_t oil = 0;
    float ironCarry = 0.0f;
    float oilCarry = 0.0f;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("iron", iron), cereal::make_nvp("oil", oil),
           cereal::make_nvp("ironCarry", ironCarry), cereal::make_nvp("oilCarry", oilCarry));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "iron", iron);
        save_detail::TryLoadValue(ar, "oil", oil);
        save_detail::TryLoadValue(ar, "ironCarry", ironCarry);
        save_detail::TryLoadValue(ar, "oilCarry", oilCarry);
    }
};

struct SaveCamera
{
    SaveVec2 target;
    SaveVec2 offset;
    float rotation = 0.0f;
    float zoom = 1.0f;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("target", target), cereal::make_nvp("offset", offset),
           cereal::make_nvp("rotation", rotation), cereal::make_nvp("zoom", zoom));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "target", target);
        save_detail::TryLoadValue(ar, "offset", offset);
        save_detail::TryLoadValue(ar, "rotation", rotation);
        save_detail::TryLoadValue(ar, "zoom", zoom);
    }
};

struct SaveTileMap
{
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::int32_t> terrain;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("width", width), cereal::make_nvp("height", height),
           cereal::make_nvp("terrain", terrain));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "width", width);
        save_detail::TryLoadValue(ar, "height", height);
        save_detail::TryLoadValue(ar, "terrain", terrain);
    }
};

struct SaveUnit
{
    float health = 0.0f;
    std::int32_t armorType = 0;
    std::int32_t damageType = 0;
    std::int32_t attackPower = 0;
    std::int32_t attackRange = 0;
    float cooldown = 0.0f;
    float cooldownTime = 0.0f;
    std::int32_t phase = 0;
    float phaseTime = 0.0f;
    float windupTime = 0.0f;
    float lastDamage = 0.0f;
    float hitFlash = 0.0f;
    float speed = 0.0f;
    float sightRange = 0.0f;
    SaveVec2 position;
    SaveVec2 velocity;
    bool selected = false;
    std::int32_t team = 0;
    std::int32_t type = 0;
    std::int32_t state = 0;
    std::int32_t targetIndex = -1;
    SaveVec2 moveTarget;
    bool hasMoveOrder = false;
    std::vector<SaveIVec2> path;
    std::uint32_t pathNext = 0;
    bool hasPath = false;
    // M2 extension state (append-only; missing -> defaults, old saves load).
    bool hasTurret = false;
    float turretFacing = 0.0f;
    std::int32_t facing = 6; // body Facing enum (Right); G1 persists both facings
    std::vector<std::int32_t> cargoManifest;
    std::int32_t embarkedOn = -1;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("health", health), cereal::make_nvp("armorType", armorType),
           cereal::make_nvp("damageType", damageType),
           cereal::make_nvp("attackPower", attackPower),
           cereal::make_nvp("attackRange", attackRange),
           cereal::make_nvp("cooldown", cooldown),
           cereal::make_nvp("cooldownTime", cooldownTime),
           cereal::make_nvp("phase", phase), cereal::make_nvp("phaseTime", phaseTime),
           cereal::make_nvp("windupTime", windupTime),
           cereal::make_nvp("lastDamage", lastDamage),
           cereal::make_nvp("hitFlash", hitFlash), cereal::make_nvp("speed", speed),
           cereal::make_nvp("sightRange", sightRange),
           cereal::make_nvp("position", position),
           cereal::make_nvp("velocity", velocity),
           cereal::make_nvp("selected", selected), cereal::make_nvp("team", team),
           cereal::make_nvp("type", type), cereal::make_nvp("state", state),
           cereal::make_nvp("targetIndex", targetIndex),
           cereal::make_nvp("moveTarget", moveTarget),
           cereal::make_nvp("hasMoveOrder", hasMoveOrder),
           cereal::make_nvp("path", path), cereal::make_nvp("pathNext", pathNext),
           cereal::make_nvp("hasPath", hasPath),
           cereal::make_nvp("hasTurret", hasTurret),
           cereal::make_nvp("turretFacing", turretFacing),
           cereal::make_nvp("facing", facing),
           cereal::make_nvp("cargoManifest", cargoManifest),
           cereal::make_nvp("embarkedOn", embarkedOn));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "health", health);
        save_detail::TryLoadValue(ar, "armorType", armorType);
        save_detail::TryLoadValue(ar, "damageType", damageType);
        save_detail::TryLoadValue(ar, "attackPower", attackPower);
        save_detail::TryLoadValue(ar, "attackRange", attackRange);
        save_detail::TryLoadValue(ar, "cooldown", cooldown);
        save_detail::TryLoadValue(ar, "cooldownTime", cooldownTime);
        save_detail::TryLoadValue(ar, "phase", phase);
        save_detail::TryLoadValue(ar, "phaseTime", phaseTime);
        save_detail::TryLoadValue(ar, "windupTime", windupTime);
        save_detail::TryLoadValue(ar, "lastDamage", lastDamage);
        save_detail::TryLoadValue(ar, "hitFlash", hitFlash);
        save_detail::TryLoadValue(ar, "speed", speed);
        save_detail::TryLoadValue(ar, "sightRange", sightRange);
        save_detail::TryLoadValue(ar, "position", position);
        save_detail::TryLoadValue(ar, "velocity", velocity);
        save_detail::TryLoadValue(ar, "selected", selected);
        save_detail::TryLoadValue(ar, "team", team);
        save_detail::TryLoadValue(ar, "type", type);
        save_detail::TryLoadValue(ar, "state", state);
        save_detail::TryLoadValue(ar, "targetIndex", targetIndex);
        save_detail::TryLoadValue(ar, "moveTarget", moveTarget);
        save_detail::TryLoadValue(ar, "hasMoveOrder", hasMoveOrder);
        save_detail::TryLoadValue(ar, "path", path);
        save_detail::TryLoadValue(ar, "pathNext", pathNext);
        save_detail::TryLoadValue(ar, "hasPath", hasPath);
        save_detail::TryLoadValue(ar, "hasTurret", hasTurret);
        save_detail::TryLoadValue(ar, "turretFacing", turretFacing);
        save_detail::TryLoadValue(ar, "facing", facing);
        save_detail::TryLoadValue(ar, "cargoManifest", cargoManifest);
        save_detail::TryLoadValue(ar, "embarkedOn", embarkedOn);
    }
};

struct SaveBuilding
{
    std::int32_t type = 0;
    std::int32_t state = 0;
    std::int32_t team = 0;
    std::int32_t tileX = 0;
    std::int32_t tileY = 0;
    float health = 0.0f;
    float maxHealth = 0.0f;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("type", type), cereal::make_nvp("state", state),
           cereal::make_nvp("team", team), cereal::make_nvp("tileX", tileX),
           cereal::make_nvp("tileY", tileY), cereal::make_nvp("health", health),
           cereal::make_nvp("maxHealth", maxHealth));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "type", type);
        save_detail::TryLoadValue(ar, "state", state);
        save_detail::TryLoadValue(ar, "team", team);
        save_detail::TryLoadValue(ar, "tileX", tileX);
        save_detail::TryLoadValue(ar, "tileY", tileY);
        save_detail::TryLoadValue(ar, "health", health);
        save_detail::TryLoadValue(ar, "maxHealth", maxHealth);
    }
};

struct SaveNode
{
    std::int32_t kind = 0;
    SaveIVec2 tile;
    float amount = 0.0f;
    float maxAmount = 0.0f;
    float respawnDelay = 0.0f;
    float respawnTimer = 0.0f;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("kind", kind), cereal::make_nvp("tile", tile),
           cereal::make_nvp("amount", amount), cereal::make_nvp("maxAmount", maxAmount),
           cereal::make_nvp("respawnDelay", respawnDelay),
           cereal::make_nvp("respawnTimer", respawnTimer));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "kind", kind);
        save_detail::TryLoadValue(ar, "tile", tile);
        save_detail::TryLoadValue(ar, "amount", amount);
        save_detail::TryLoadValue(ar, "maxAmount", maxAmount);
        save_detail::TryLoadValue(ar, "respawnDelay", respawnDelay);
        save_detail::TryLoadValue(ar, "respawnTimer", respawnTimer);
    }
};

struct SaveFog
{
    std::int32_t team = 0;
    // Raw explored bytes as numbers: cereal JSON truncates std::string at
    // the first NUL on load, so binary blobs must be numeric arrays.
    std::vector<std::uint8_t> explored;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("team", team), cereal::make_nvp("explored", explored));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        save_detail::TryLoadValue(ar, "team", team);
        save_detail::TryLoadValue(ar, "explored", explored);
    }
};

struct SaveGameData
{
    std::uint32_t saveVersion = 0;
    SaveResources resources;
    SaveCamera camera;
    SaveTileMap map;
    std::vector<SaveUnit> units;
    std::vector<SaveBuilding> buildings;
    std::vector<SaveNode> nodes;
    float nodeIronCarry = 0.0f;
    float nodeOilCarry = 0.0f;
    std::vector<SaveFog> teamFog;
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("saveVersion", saveVersion),
           cereal::make_nvp("resources", resources), cereal::make_nvp("camera", camera),
           cereal::make_nvp("map", map), cereal::make_nvp("units", units),
           cereal::make_nvp("buildings", buildings), cereal::make_nvp("nodes", nodes),
           cereal::make_nvp("nodeIronCarry", nodeIronCarry),
           cereal::make_nvp("nodeOilCarry", nodeOilCarry),
           cereal::make_nvp("teamFog", teamFog));
    }
    template <class Archive>
    void load(Archive &ar)
    {
        // saveVersion is required: a versionless payload is not a save.
        ar(cereal::make_nvp("saveVersion", saveVersion));
        save_detail::TryLoadValue(ar, "resources", resources);
        save_detail::TryLoadValue(ar, "camera", camera);
        save_detail::TryLoadValue(ar, "map", map);
        save_detail::TryLoadValue(ar, "units", units);
        save_detail::TryLoadValue(ar, "buildings", buildings);
        save_detail::TryLoadValue(ar, "nodes", nodes);
        save_detail::TryLoadValue(ar, "nodeIronCarry", nodeIronCarry);
        save_detail::TryLoadValue(ar, "nodeOilCarry", nodeOilCarry);
        save_detail::TryLoadValue(ar, "teamFog", teamFog);
    }
};
