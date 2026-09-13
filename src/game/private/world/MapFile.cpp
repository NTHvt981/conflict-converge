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
    if (data.width <= 0 || data.height <= 0 ||
        static_cast<std::int64_t>(data.width) * data.height > kMaxTiles)
    {
        return false;
    }
    map.Resize(data.width, data.height);
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
