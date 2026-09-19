#pragma once

#include <string>
#include <vector>

#include "MathUtils.h"
#include "Nodes.h"
#include "TileMap.h"

class ResourceNodes;
class OccupancyGrid;

// Text `.map` format: 5-line header (comment, name, author, width, height),
// then exactly height rows of exactly width legend characters.

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
    std::vector<TerrainType> terrain; // row-major, width*height
    std::vector<MapNodeSpawn> nodes;
    std::vector<cc::IVec2> playerSpawns; // '1' markers
    std::vector<cc::IVec2> aiSpawns;     // '2' markers
};

// Parse a .map file; false leaves `out` untouched.
bool ParseMapFile(const std::string &path, MapData &out);

// Materialize parsed data: Resize + terrain, node spawns, building tiles.
// False only on dimension overflow. The OccupancyGrid overload resyncs and
// clears the grid to the new dims.
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes);
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes,
                  OccupancyGrid *occ);

// QoL map editor: inverse of ParseMapFile. False (writing nothing) on data
// that would not load back.
bool WriteMapFile(const MapData &data, const std::string &path);

// QoL map editor: playability check (warns, never blocks). False sets
// *warning to the first failed check.
bool ValidateMapPlayable(const MapData &data, std::string *warning);

// QoL map editor painter: stamps one legend character onto a tile.
void PaintEditorCell(MapData &map, char brush, cc::IVec2 tile);

// QoL map editor Save-As: filename guard before WriteMapFile.
bool IsValidMapSaveName(const std::string &name);

// Skirmish-setup map entry (header metadata plus file path).
struct MapEntry
{
    std::string path;
    std::string name;
    std::string author;
    int width = 0;
    int height = 0;
};

// Enumerate dir/*.map, keeping only files that parse; sorted by path.
std::vector<MapEntry> ListMaps(const std::string &dir);

// Nearest walkable tile within a small spiral (avoid spawning inside
// footprints); falls back to the requested tile.
cc::IVec2 NearestFreeTile(const TileMap &map, int tileX, int tileY);
