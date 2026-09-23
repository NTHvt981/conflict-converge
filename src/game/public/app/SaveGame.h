#pragma once

#include <string>

class Registry;
class ResourceSystem;
class TileMap;
class OccupancyGrid;
class GameCamera;
class ResourceNodes;
class FogOfWar;

// Versioned JSON save/load. Payload is cereal-JSON ("CCJ3" magic, then one
// SaveGameData; wire structs in ../private/app/SaveWire.h). Field loads are
// tolerant (missing keys take defaults), so old saves keep loading as new
// fields are appended; versions outside [kMinSupportedVersion, kSaveVersion]
// and unknown enum values still reject the load.
inline constexpr unsigned int kSaveVersion = 3;
inline constexpr unsigned int kMinSupportedVersion = 3;

struct WorldState
{
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;
    TileMap *map = nullptr;
    GameCamera *camera = nullptr;
    ResourceNodes *nodes = nullptr;
    FogOfWar *fog = nullptr; // per-team explored memory (saved via team_fog)
    // Tile occupancy; optional (null for bare tests). LoadWorld resyncs it to
    // the save's dims when present.
    OccupancyGrid *occ = nullptr;
};

// Every WorldState pointer must be non-null. Save returns false on I/O
// errors. Load returns false (leaving the destination world untouched) on
// I/O errors, bad magic, unsupported versions, or garbage data.
bool SaveWorld(const WorldState &world, const std::string &path);
bool LoadWorld(const WorldState &world, const std::string &path);

// Named save slots (1..3, clamped) under data/.
std::string SaveSlotPath(int slot);

// QoL snapshot replays: periodic full-state captures, one file per frame.
inline const char *kReplayDir = "data/replays";
inline constexpr int kReplayMaxFrames = 300; // 10 min at the 2s cadence
// Frame file path (zero-padded, sorts lexically = chronologically).
std::string ReplayFramePath(const std::string &dir, int index);
// How many contiguous frames exist starting at 0 (stops at the first gap).
int ReplayFrameCount(const std::string &dir);
