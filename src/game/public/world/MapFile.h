#pragma once

#include <string>
#include <vector>

#include "MathUtils.h" // cc::IVec2
#include "Nodes.h"     // ResourceKind for node markers
#include "TileMap.h"   // TerrainType grid

class ResourceNodes; // fwd-decl (MapFile.cpp includes Nodes.h)
class OccupancyGrid; // fwd-decl (TileMap.h defines it; resynced on apply)

// Text `.map` format.
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
// dimension overflow. The OccupancyGrid overload additionally resyncs (and
// clears) the grid to the new dims so stale widths can't corrupt footprint
// reservations; pass nullptr to keep the legacy 3-arg behavior.
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes);
bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes,
                  OccupancyGrid *occ);

// QoL map editor: inverse of ParseMapFile. Emits the documented format
// exactly (5-line header + grid rows). Nodes/spawns overlay their legend
// characters onto grass tiles; `B` comes from Building terrain. False (and
// writes nothing) on dimension overflow or terrain/node/spawn lists that
// don't fit the declared dims — editor output must never fail to load.
bool WriteMapFile(const MapData &data, const std::string &path);

// QoL map editor: playability check (does NOT gate WriteMapFile — the
// editor warns instead of blocking). True when the dims parse, at least one
// player and one AI spawn exist, every node is reachable on foot from the
// first player spawn, and both homes connect. On false, *warning (when
// non-null) names the first failed check.
bool ValidateMapPlayable(const MapData &data, std::string *warning);

// QoL map editor painter: stamps one legend character ('.', '~', 'T',
// '^', 'B', 'I', 'O', '1', '2'; anything else ignored) onto a tile.
// Markers ('I'/'O'/'1'/'2') sit on grass with an entry appended to the
// matching list; painting anything else first clears that tile's marker
// entries. Out-of-bounds tiles are ignored.
void PaintEditorCell(MapData &map, char brush, cc::IVec2 tile);

// QoL map editor Save-As: filename guard before WriteMapFile. True for a
// bare stem ("custom", "my_map-2") — non-empty, no separators, no drive
// colon, no parent-directory escape. The game prefixes "data/maps/" and
// appends ".map" itself, so anything else is rejected.
bool IsValidMapSaveName(const std::string &name);

// Skirmish-setup map entry — header metadata plus the file path.
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
