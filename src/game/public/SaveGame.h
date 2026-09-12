#pragma once

#include <string>

class Registry;
class ResourceSystem;
class TileMap;
class GameCamera;
class ResourceNodes;

// M7: versioned binary save/load. Layout (little-endian, Windows x86 target):
//   "CCSV" magic, uint32 version, then resources (+carry), camera, map,
//   units, buildings, nodes (+carry).
// Entity IDs are session-local: unit targets are stored as indices into the
// saved unit array (-1 = none) and remapped to fresh IDs on load. Production
// queues, menu state, and UI toggles are session-only and NOT saved.
inline constexpr unsigned int kSaveVersion = 1;

struct WorldState
{
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;
    TileMap *map = nullptr;
    GameCamera *camera = nullptr;
    ResourceNodes *nodes = nullptr;
};

// Every WorldState pointer must be non-null. Save returns false on I/O
// errors. Load returns false (leaving the destination world untouched) on
// I/O errors, bad magic, unsupported versions, or truncated/garbage data.
bool SaveWorld(const WorldState &world, const std::string &path);
bool LoadWorld(const WorldState &world, const std::string &path);
