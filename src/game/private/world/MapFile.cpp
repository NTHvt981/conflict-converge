#include "MapFile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Nodes.h"

namespace
{

constexpr float kDefaultIron = 200.0f;
constexpr float kDefaultOil = 150.0f;
constexpr float kDefaultRespawn = 10.0f;
constexpr int kMaxDim = 1024;
constexpr std::int64_t kMaxTiles = 1024 * 1024;

bool ParseDim(const std::string &line, const char *key, int &out)
{
    const std::string prefix = std::string(key) + "=";
    if (line.rfind(prefix, 0) != 0)
    {
        return false;
    }
    std::istringstream rest(line.substr(prefix.size()));
    int value = 0;
    char extra = 0;
    if (!(rest >> value) || (rest >> extra) || value <= 0 || value > kMaxDim)
    {
        return false;
    }
    out = value;
    return true;
}

bool ParseValue(const std::string &line, const char *key, std::string &out)
{
    const std::string prefix = std::string(key) + "=";
    if (line.rfind(prefix, 0) != 0)
    {
        return false;
    }
    out = line.substr(prefix.size());
    return true;
}

} // namespace

bool ParseMapFile(const std::string &path, MapData &out)
{
    std::ifstream file(path);
    if (!file)
    {
        return false;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (lines.size() < 5 || lines[0].empty() || lines[0][0] != '#')
    {
        return false;
    }
    MapData data;
    if (!ParseValue(lines[1], "name", data.name) || data.name.empty() ||
        !ParseValue(lines[2], "author", data.author) || !ParseDim(lines[3], "width", data.width) ||
        !ParseDim(lines[4], "height", data.height))
    {
        return false;
    }
    if (static_cast<std::int64_t>(data.width) * data.height > kMaxTiles)
    {
        return false;
    }
    if (static_cast<int>(lines.size()) != 5 + data.height)
    {
        return false; // grid must be exactly height rows, nothing trailing
    }
    data.terrain.assign(static_cast<std::size_t>(data.width) * data.height, TerrainType::Grass);
    for (int y = 0; y < data.height; ++y)
    {
        const std::string &row = lines[5 + y];
        if (static_cast<int>(row.size()) != data.width)
        {
            return false;
        }
        for (int x = 0; x < data.width; ++x)
        {
            const cc::IVec2 tile{ x, y };
            TerrainType terrain = TerrainType::Grass;
            switch (row[x])
            {
            case '.':
                break;
            case '~':
                terrain = TerrainType::Water;
                break;
            case 'T':
                terrain = TerrainType::Forest;
                break;
            case '^':
                terrain = TerrainType::Rock;
                break;
            case 'B':
                terrain = TerrainType::Building;
                break;
            case 'I':
                data.nodes.push_back({ ResourceKind::Iron, tile });
                break;
            case 'O':
                data.nodes.push_back({ ResourceKind::Oil, tile });
                break;
            case '1':
                data.playerSpawns.push_back(tile);
                break;
            case '2':
                data.aiSpawns.push_back(tile);
                break;
            default:
                return false; // unknown legend character
            }
            data.terrain[static_cast<std::size_t>(y) * data.width + x] = terrain;
        }
    }
    out = std::move(data);
    return true;
}

bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes)
{
    return ApplyMapData(data, map, nodes, nullptr);
}

bool ApplyMapData(const MapData &data, TileMap &map, ResourceNodes &nodes,
                  OccupancyGrid *occ)
{
    if (data.width <= 0 || data.height <= 0 ||
        static_cast<std::int64_t>(data.width) * data.height > kMaxTiles)
    {
        return false;
    }
    map.Resize(data.width, data.height);
    if (occ != nullptr)
    {
        occ->Resize(data.width, data.height);
    }
    for (int y = 0; y < data.height; ++y)
    {
        for (int x = 0; x < data.width; ++x)
        {
            map.Set({ x, y }, data.terrain[static_cast<std::size_t>(y) * data.width + x]);
        }
    }
    for (const MapNodeSpawn &spawn : data.nodes)
    {
        const float amount = spawn.kind == ResourceKind::Iron ? kDefaultIron : kDefaultOil;
        nodes.SpawnNode(map, spawn.kind, spawn.tile, amount, kDefaultRespawn);
    }
    return true;
}

namespace
{

bool EditorDimsValid(const MapData &data)
{
    if (data.width <= 0 || data.width > kMaxDim || data.height <= 0 ||
        data.height > kMaxDim)
    {
        return false;
    }
    if (static_cast<std::int64_t>(data.width) * data.height > kMaxTiles)
    {
        return false;
    }
    return data.terrain.size() ==
           static_cast<std::size_t>(data.width) * static_cast<std::size_t>(data.height);
}

bool EditorTileInMap(const MapData &data, cc::IVec2 tile)
{
    return tile.x >= 0 && tile.y >= 0 && tile.x < data.width && tile.y < data.height;
}

bool EditorPassable(const MapData &data, cc::IVec2 tile)
{
    if (!EditorTileInMap(data, tile))
    {
        return false;
    }
    const TerrainType terrain =
        data.terrain[static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(data.width) +
                     static_cast<std::size_t>(tile.x)];
    return terrain != TerrainType::Water && terrain != TerrainType::Building &&
           terrain != TerrainType::Rock;
}

// 4-dir flood fill over passable tiles (mirrors TileMap::IsBlocked minus
// the out-of-bounds rule, which is handled by EditorTileInMap above).
bool EditorReachable(const MapData &data, cc::IVec2 from, cc::IVec2 to)
{
    if (!EditorPassable(data, from) || !EditorPassable(data, to))
    {
        return false;
    }
    std::vector<char> seen(static_cast<std::size_t>(data.width) * data.height, 0);
    std::vector<cc::IVec2> stack{ from };
    seen[static_cast<std::size_t>(from.y) * data.width + from.x] = 1;
    const int kDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    while (!stack.empty())
    {
        const cc::IVec2 cur = stack.back();
        stack.pop_back();
        if (cur == to)
        {
            return true;
        }
        for (const auto &d : kDirs)
        {
            const cc::IVec2 next{ cur.x + d[0], cur.y + d[1] };
            if (!EditorPassable(data, next))
            {
                continue;
            }
            const std::size_t i =
                static_cast<std::size_t>(next.y) * data.width + next.x;
            if (seen[i] == 0)
            {
                seen[i] = 1;
                stack.push_back(next);
            }
        }
    }
    return false;
}

} // namespace

bool WriteMapFile(const MapData &data, const std::string &path)
{
    if (!EditorDimsValid(data) || data.name.empty())
    {
        return false;
    }
    // Node/spawn markers must sit on grass tiles inside the map (the
    // reader rejects anything else, so the writer refuses first).
    for (const MapNodeSpawn &spawn : data.nodes)
    {
        if (!EditorTileInMap(data, spawn.tile))
        {
            return false;
        }
    }
    for (const cc::IVec2 &tile : data.playerSpawns)
    {
        if (!EditorTileInMap(data, tile))
        {
            return false;
        }
    }
    for (const cc::IVec2 &tile : data.aiSpawns)
    {
        if (!EditorTileInMap(data, tile))
        {
            return false;
        }
    }
    // Overlay markers onto a grass canvas; `B` comes from terrain.
    std::vector<std::string> rows(static_cast<std::size_t>(data.height),
                                  std::string(static_cast<std::size_t>(data.width), '.'));
    for (int y = 0; y < data.height; ++y)
    {
        for (int x = 0; x < data.width; ++x)
        {
            const TerrainType terrain =
                data.terrain[static_cast<std::size_t>(y) * data.width + x];
            char cell = '.';
            switch (terrain)
            {
            case TerrainType::Grass:
                cell = '.';
                break;
            case TerrainType::Water:
                cell = '~';
                break;
            case TerrainType::Forest:
                cell = 'T';
                break;
            case TerrainType::Rock:
                cell = '^';
                break;
            case TerrainType::Building:
                cell = 'B';
                break;
            }
            rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = cell;
        }
    }
    for (const MapNodeSpawn &spawn : data.nodes)
    {
        rows[static_cast<std::size_t>(spawn.tile.y)][static_cast<std::size_t>(spawn.tile.x)] =
            spawn.kind == ResourceKind::Iron ? 'I' : 'O';
    }
    for (const cc::IVec2 &tile : data.playerSpawns)
    {
        rows[static_cast<std::size_t>(tile.y)][static_cast<std::size_t>(tile.x)] = '1';
    }
    for (const cc::IVec2 &tile : data.aiSpawns)
    {
        rows[static_cast<std::size_t>(tile.y)][static_cast<std::size_t>(tile.x)] = '2';
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return false;
    }
    file << "# Conflict Converge map v1\n";
    file << "name=" << data.name << "\n";
    file << "author=" << data.author << "\n";
    file << "width=" << data.width << "\n";
    file << "height=" << data.height << "\n";
    for (const std::string &row : rows)
    {
        file << row << "\n";
    }
    file.flush();
    return static_cast<bool>(file);
}

bool ValidateMapPlayable(const MapData &data, std::string *warning)
{
    auto fail = [&](const std::string &message) {
        if (warning != nullptr)
        {
            *warning = message;
        }
        return false;
    };
    if (!EditorDimsValid(data) || data.name.empty())
    {
        return fail("dimensions or name invalid");
    }
    if (data.playerSpawns.empty())
    {
        return fail("no player spawn ('1' marker)");
    }
    if (data.aiSpawns.empty())
    {
        return fail("no AI spawn ('2' marker)");
    }
    if (!EditorReachable(data, data.playerSpawns[0], data.aiSpawns[0]))
    {
        return fail("player and AI homes do not connect");
    }
    for (const MapNodeSpawn &spawn : data.nodes)
    {
        if (!EditorReachable(data, data.playerSpawns[0], spawn.tile))
        {
            return fail("a resource node is unreachable from the player home");
        }
    }
    return true;
}

void PaintEditorCell(MapData &map, char brush, cc::IVec2 tile)
{
    if (!EditorTileInMap(map, tile))
    {
        return;
    }
    // One marker per tile: painting anything clears node/spawn entries
    // here first (terrain markers re-add below as needed).
    map.nodes.erase(std::remove_if(map.nodes.begin(), map.nodes.end(),
                                   [&](const MapNodeSpawn &spawn) {
                                       return spawn.tile == tile;
                                   }),
                    map.nodes.end());
    map.playerSpawns.erase(std::remove(map.playerSpawns.begin(), map.playerSpawns.end(), tile),
                           map.playerSpawns.end());
    map.aiSpawns.erase(std::remove(map.aiSpawns.begin(), map.aiSpawns.end(), tile),
                       map.aiSpawns.end());
    const std::size_t i =
        static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(map.width) +
        static_cast<std::size_t>(tile.x);
    switch (brush)
    {
    case '~':
        map.terrain[i] = TerrainType::Water;
        break;
    case 'T':
        map.terrain[i] = TerrainType::Forest;
        break;
    case '^':
        map.terrain[i] = TerrainType::Rock;
        break;
    case 'B':
        map.terrain[i] = TerrainType::Building;
        break;
    case 'I':
        map.terrain[i] = TerrainType::Grass;
        map.nodes.push_back({ ResourceKind::Iron, tile });
        break;
    case 'O':
        map.terrain[i] = TerrainType::Grass;
        map.nodes.push_back({ ResourceKind::Oil, tile });
        break;
    case '1':
        map.terrain[i] = TerrainType::Grass;
        map.playerSpawns.push_back(tile);
        break;
    case '2':
        map.terrain[i] = TerrainType::Grass;
        map.aiSpawns.push_back(tile);
        break;
    case '.':
    default:
        map.terrain[i] = TerrainType::Grass;
        break;
    }
}

bool IsValidMapSaveName(const std::string &name)
{
    if (name.empty() || name.size() > 48)
    {
        return false;
    }
    if (name == "." || name == ".." || name.find("..") != std::string::npos)
    {
        return false;
    }
    for (char c : name)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ';
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

cc::IVec2 NearestFreeTile(const TileMap &map, int tileX, int tileY)
{
    const cc::IVec2 want{ tileX, tileY };
    if (map.InBounds(want) && !map.IsBlocked(want))
    {
        return want;
    }
    for (int ring = 1; ring <= 5; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                const cc::IVec2 tile{ tileX + dx, tileY + dy };
                if (map.InBounds(tile) && !map.IsBlocked(tile))
                {
                    return tile;
                }
            }
        }
    }
    return want;
}

std::vector<MapEntry> ListMaps(const std::string &dir)
{
    std::vector<MapEntry> maps;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        return maps; // missing/unreadable dir: setup shows an empty list
    }
    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec))
    {
        if (ec || !it->is_regular_file(ec) || it->path().extension() != ".map")
        {
            continue;
        }
        MapData data;
        if (!ParseMapFile(it->path().string(), data))
        {
            continue; // unparseable files never reach the setup screen
        }
        MapEntry entry;
        entry.path = it->path().string();
        entry.name = data.name.empty() ? it->path().stem().string() : data.name;
        entry.author = data.author;
        entry.width = data.width;
        entry.height = data.height;
        maps.push_back(std::move(entry));
    }
    std::sort(maps.begin(), maps.end(),
              [](const MapEntry &a, const MapEntry &b) { return a.path < b.path; });
    return maps;
}
