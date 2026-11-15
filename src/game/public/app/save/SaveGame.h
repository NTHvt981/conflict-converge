#pragma once

#include <string>

class Registry;
class ResourceSystem;
class TileMap;
class OccupancyGrid;
class GameCamera;
class ResourceNodes;
class FogOfWar;

// Versioned JSON saves: "CCJ3" magic, then one SaveGameData (wire structs in
// save/SaveWire.h). Versions outside [kMinSupportedVersion, kSaveVersion] reject.
inline constexpr unsigned int kSaveVersion = 4;
inline constexpr unsigned int kMinSupportedVersion = 3;
inline constexpr char kMagic[4] = { 'C', 'C', 'J', '3' };

struct WorldState
{
	Registry *registry = nullptr;
	ResourceSystem *resources = nullptr;
	TileMap *map = nullptr;
	GameCamera *camera = nullptr;
	ResourceNodes *nodes = nullptr;
	FogOfWar *fog = nullptr; // per-team explored memory
	// Tile occupancy; optional (null for bare tests). LoadWorld resyncs it to the save's dims.
	OccupancyGrid *occ = nullptr;
};

// Every WorldState pointer must be non-null. Failures return false; a failed
// Load leaves the destination world untouched.
bool SaveWorld(const WorldState &world, const std::string &path);
bool LoadWorld(const WorldState &world, const std::string &path);

// Named save slots (1..3, clamped) under data/.
std::string SaveSlotPath(int slot);

// QoL snapshot replays: periodic full-state captures, one file per frame.
inline const char *kReplayDir = "data/replays";
inline constexpr int kReplayMaxFrames = 300; // 10 min at the 2s cadence
// Frame file path (zero-padded, sorts lexically = chronologically).
std::string ReplayFramePath(const std::string &dir, int index);
// Contiguous frames from 0; stops at the first gap.
int ReplayFrameCount(const std::string &dir);
