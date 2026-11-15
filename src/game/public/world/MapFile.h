#pragma once

#include <string>
#include <vector>

#include "core/MathUtils.h"
#include "economy/Nodes.h"
#include "world/TileMap.h"

class ResourceNodes;
class OccupancyGrid;

struct MapNodeSpawn
{
	ResourceKind kind = ResourceKind::Iron;
	cc::IVec2 tile{ 0, 0 };
};

struct MapData
{
	std::string name;
	std::string author;
	int width = 0;
	int height = 0;
	std::vector<TerrainType> terrain;
	std::vector<MapNodeSpawn> nodes;
	std::vector<cc::IVec2> playerSpawns;
	std::vector<cc::IVec2> aiSpawns;
};

// False leaves `out` untouched.
bool ParseMapFile(const std::string &path, MapData &out);

// False only on dimension overflow; the OccupancyGrid overload clears the grid to
// the new dims.
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes);
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes,
				  OccupancyGrid *occ);

// Inverse of ParseMapFile; writes nothing and returns false on data that would
// not reload.
bool WriteMapFile(const MapData &data, const std::string &path);

// Warns, never blocks; false sets *warning to the first failed check.
bool ValidateMapPlayable(const MapData &data, std::string *warning);

void PaintEditorCell(MapData &map, char brush, cc::IVec2 tile);

bool IsValidMapSaveName(const std::string &name);

struct MapEntry
{
	std::string path;
	std::string name;
	std::string author;
	int width = 0;
	int height = 0;
};

std::vector<MapEntry> ListMaps(const std::string &dir);

// Small spiral search; falls back to the requested tile.
cc::IVec2 NearestFreeTile(const TileMap &map, int tileX, int tileY);
