#pragma once

#include <string>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Nodes.h"     // ResourceKind for node markers
#include "TileMap.h"   // TerrainType grid

class ResourceNodes; // fwd-decl (MapFile.cpp includes Nodes.h)

// M10: text `.map` format (Q46 text, Q54 header, Q73 data/ loading, Q79 legend).
// Strict layout — comment, name, author, width, height, then exactly
// height rows of exactly width characters:
//   # Conflict Converge map v1
//   name=Crossroads
//   author=
//   width=24
//   height=18
//   ........................
// Legend: `.` grass, `~` water, `T` forest, `^` rock, `I`/`O` iron/oil node
// (on grass), `1`/`2` player/AI spawn (on grass), `B` pre-placed Building
// tile. Anything else — including ragged rows — rejects the file.

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

// Parse a .map file. False on missing/unreadable files, bad headers,
// unknown legend characters, or dimension mismatches; `out` untouched then.
bool ParseMapFile(const std::string &path, MapData &out);

// Materialize parsed data: Resize + terrain, node spawns (default amounts),
// `B` tiles already baked into terrain. Markers stay in MapData for the
// caller (demo places bases/units relative to them). False only on
// dimension overflow.
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes);

// M14: skirmish-setup map entry — header metadata plus the file path.
// ListMaps enumerates dir/*.map, keeping only files that parse (header
// gives name/author/dimensions for the setup screen). Sorted by path so
// the list is stable across frames; empty when the dir is missing.
struct MapEntry
{
    std::string path;
    std::string name;
    std::string author;
    int width = 0;
    int height = 0;
};

std::vector<MapEntry> ListMaps(const std::string &dir);

// Nearest walkable tile to (tileX, tileY) within a small spiral (units must
// never spawn inside fresh Building footprints). Falls back to the requested
// tile when nothing walkable is near.
cc::IVec2 NearestFreeTile(const TileMap &map, int tileX, int tileY);
