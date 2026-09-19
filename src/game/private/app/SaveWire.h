#pragma once

// Cereal wire structs for the save format (replaces proto/savegame.proto).
// Kept separate from the live gameplay structs on purpose: the wire shape
// must stay independently versionable and must tolerate garbage without
// touching live game state until fully validated (Decode parses into
// SavedWorld snapshots first, in SaveGame.cpp). Symmetric serialize() is
// enough — unlike the JSON configs, the binary archive always reads back
// exactly what was written, so no presence tracking is needed.
// Field order is the on-disk order: appending a field is backward-readable
// only if Decode never assumes a length, so treat any shape change as a
// new save_version (see kSaveVersion in SaveGame.h).

#include <cstdint>
#include <string>
#include <vector>

struct SaveVec2
{
    float x = 0.0f;
    float y = 0.0f;
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(x, y);
    }
};

struct SaveIVec2
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(x, y);
    }
};

struct SaveResources
{
    std::int64_t iron = 0;
    std::int64_t oil = 0;
    float ironCarry = 0.0f;
    float oilCarry = 0.0f;
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(iron, oil, ironCarry, oilCarry);
    }
};

struct SaveCamera
{
    SaveVec2 target;
    SaveVec2 offset;
    float rotation = 0.0f;
    float zoom = 1.0f;
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(target, offset, rotation, zoom);
    }
};

struct SaveTileMap
{
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::int32_t> terrain;
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(width, height, terrain);
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
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(health, armorType, damageType, attackPower, attackRange, cooldown, cooldownTime,
           phase, phaseTime, windupTime, lastDamage, hitFlash, speed, sightRange, position,
           velocity, selected, team, type, state, targetIndex, moveTarget, hasMoveOrder, path,
           pathNext, hasPath);
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
    void serialize(Archive &ar)
    {
        ar(type, state, team, tileX, tileY, health, maxHealth);
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
    void serialize(Archive &ar)
    {
        ar(kind, tile, amount, maxAmount, respawnDelay, respawnTimer);
    }
};

struct SaveFog
{
    std::int32_t team = 0;
    std::string explored;
    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(team, explored);
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
    void serialize(Archive &ar)
    {
        ar(saveVersion, resources, camera, map, units, buildings, nodes, nodeIronCarry,
           nodeOilCarry, teamFog);
    }
};
