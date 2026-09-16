#pragma once

#include <string>

class Registry;
class ResourceSystem;
class TileMap;
class OccupancyGrid;
class GameCamera;
class ResourceNodes;
class FogOfWar;

// M7: versioned binary save/load. M15 migrated the payload to protobuf
// (proto/savegame.proto, checked-in generated code in ../private/):
//   "CCPB" magic, then one serialized cc.save.SaveGame message.
// Entity IDs are session-local: unit targets are stored as indices into the
// saved unit array (-1 = none) and remapped to fresh IDs on load. Production
// queues, menu state, and UI toggles are session-only and NOT saved.
// Old "CCSV" custom-binary files are rejected by the magic check.
inline constexpr unsigned int kSaveVersion = 1;

struct WorldState
{
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;
    TileMap *map = nullptr;
    GameCamera *camera = nullptr;
    ResourceNodes *nodes = nullptr;
    FogOfWar *fog = nullptr; // M9: per-team explored memory (saved via team_fog)
    // Phase 4: tile occupancy. Optional (defaults null for bare tests):
    // LoadWorld resyncs it to the save's dims when present so a stale
    // width can't corrupt footprint reservations across a load.
    OccupancyGrid *occ = nullptr;
};

// Every WorldState pointer must be non-null. Save returns false on I/O
// errors. Load returns false (leaving the destination world untouched) on
// I/O errors, bad magic, unsupported versions, or truncated/garbage data.
bool SaveWorld(const WorldState &world, const std::string &path);
bool LoadWorld(const WorldState &world, const std::string &path);

// M13: named save slots (1..3, clamped) under data/. The F5 quicksave path
// stays separate.
std::string SaveSlotPath(int slot);
