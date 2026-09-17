// Unit tests for M10 map files (format validation, shipped-map sanity,
// save roundtrip on a loaded map).

#include "test_harness.h"

#include "AICommander.h" // demo smoke: SetupBase must fit on shipped maps
#include "Building.h" // demo smoke: player placements must fit
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "MapFile.h"
#include "Nodes.h"
#include "Pathfinder.h" // TerrainCost hook
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "TileMap.h"

#include <filesystem>
#include <fstream>
#include <queue>

namespace
{

std::string TempMap(const std::string &name)
{
    return (std::filesystem::temp_directory_path() / name).string();
}

void WriteFile(const std::string &path, const std::string &content)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

bool Reachable(const TileMap &map, cc::IVec2 from, cc::IVec2 to)
{
    if (!map.InBounds(from) || !map.InBounds(to))
    {
        return false;
    }
    std::vector<char> seen(static_cast<std::size_t>(map.Width()) * map.Height(), 0);
    std::queue<cc::IVec2> open;
    open.push(from);
    seen[static_cast<std::size_t>(from.y) * map.Width() + from.x] = 1;
    const cc::IVec2 dirs[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    while (!open.empty())
    {
        const cc::IVec2 cur = open.front();
        open.pop();
        if (cur == to)
        {
            return true;
        }
        for (cc::IVec2 d : dirs)
        {
            const cc::IVec2 next{ cur.x + d.x, cur.y + d.y };
            if (!map.InBounds(next) || map.IsBlocked(next))
            {
                continue;
            }
            const std::size_t i = static_cast<std::size_t>(next.y) * map.Width() + next.x;
            if (!seen[i])
            {
                seen[i] = 1;
                open.push(next);
            }
        }
    }
    return false;
}

// Shipped maps live in data/maps at repo root; the test binary may run from
// repo root or prj/bin/<Config>, so probe both relative spellings.
bool LoadShipped(const std::string &name, MapData &out)
{
    const std::string candidates[] = { "data/maps/" + name, "../../data/maps/" + name,
                                       "../../../data/maps/" + name };
    for (const std::string &path : candidates)
    {
        if (ParseMapFile(path, out))
        {
            return true;
        }
    }
    return false;
}

const char *kValidMap = "# Conflict Converge map v1\n"
                        "name=Probe\n"
                        "author=test\n"
                        "width=4\n"
                        "height=3\n"
                        ".1~.\n"
                        ".TI2\n"
                        ".^BO\n";

} // namespace

void RunMapFileTests()
{
    // --- terrain types + cost hook ---
    CC_CHECK(!TileMap(4, 4).IsBlocked({ 0, 0 })); // grass
    TileMap blocked(4, 4);
    blocked.Set({ 0, 0 }, TerrainType::Forest);
    blocked.Set({ 1, 0 }, TerrainType::Rock);
    CC_CHECK(!blocked.IsBlocked({ 0, 0 })); // forest passable
    CC_CHECK(blocked.IsBlocked({ 1, 0 }));  // rock walls
    CC_CHECK(TerrainCost(TerrainType::Grass) == 1.0f);
    CC_CHECK(TerrainCost(TerrainType::Forest) == 1.0f); // uniform per Q55
    CC_CHECK(TerrainCost(TerrainType::Water) == 1.0f);

    // --- NearestFreeTile: walkable spawn points around footprints ---
    TileMap freeMap(8, 8);
    CC_CHECK(NearestFreeTile(freeMap, 3, 3) == cc::IVec2(3, 3)); // open ground
    freeMap.Set({ 3, 3 }, TerrainType::Building);
    const cc::IVec2 near = NearestFreeTile(freeMap, 3, 3);
    CC_CHECK(!(near == cc::IVec2(3, 3)));
    CC_CHECK(!freeMap.IsBlocked(near)); // some adjacent grass tile
    TileMap solid(4, 4);
    solid.Clear(TerrainType::Water);
    CC_CHECK(NearestFreeTile(solid, 1, 1) == cc::IVec2(1, 1)); // nowhere: fallback

    // --- valid parse: every legend character lands correctly ---
    const std::string good = TempMap("cc_map_good.map");
    WriteFile(good, kValidMap);
    MapData data;
    CC_CHECK(ParseMapFile(good, data));
    CC_CHECK(data.name == "Probe" && data.author == "test");
    CC_CHECK(data.width == 4 && data.height == 3);
    CC_CHECK(data.terrain[1] == TerrainType::Grass); // '1' marker sits on grass
    CC_CHECK(data.terrain[2] == TerrainType::Water);
    CC_CHECK(data.terrain[5] == TerrainType::Forest);
    CC_CHECK(data.terrain[6] == TerrainType::Grass); // 'I' sits on grass
    CC_CHECK(data.terrain[7] == TerrainType::Grass); // '2' sits on grass
    CC_CHECK(data.terrain[9] == TerrainType::Rock);
    CC_CHECK(data.terrain[10] == TerrainType::Building);
    CC_CHECK(data.terrain[11] == TerrainType::Grass); // 'O' sits on grass
    CC_CHECK(data.playerSpawns.size() == 1 && data.playerSpawns[0] == cc::IVec2(1, 0));
    CC_CHECK(data.aiSpawns.size() == 1 && data.aiSpawns[0] == cc::IVec2(3, 1));
    CC_CHECK(data.nodes.size() == 2);

    // --- rejection paths ---
    MapData bad;
    CC_CHECK(!ParseMapFile(TempMap("cc_map_missing.map"), bad)); // absent file
    const std::string badChar = TempMap("cc_map_badchar.map");
    WriteFile(badChar, "# c\nname=N\nauthor=\nwidth=2\nheight=1\n.X\n");
    CC_CHECK(!ParseMapFile(badChar, bad));
    const std::string shortRow = TempMap("cc_map_shortrow.map");
    WriteFile(shortRow, "# c\nname=N\nauthor=\nwidth=2\nheight=1\n.\n");
    CC_CHECK(!ParseMapFile(shortRow, bad));
    const std::string rowCount = TempMap("cc_map_rowcount.map");
    WriteFile(rowCount, "# c\nname=N\nauthor=\nwidth=2\nheight=2\n..\n..\n..\n");
    CC_CHECK(!ParseMapFile(rowCount, bad));
    const std::string badHeader = TempMap("cc_map_badheader.map");
    WriteFile(badHeader, "# c\nname=N\nauthor=\nwidth=2\n");
    CC_CHECK(!ParseMapFile(badHeader, bad));
    const std::string badDim = TempMap("cc_map_baddim.map");
    WriteFile(badDim, "# c\nname=N\nauthor=\nwidth=0\nheight=1\n.\n");
    CC_CHECK(!ParseMapFile(badDim, bad));
    const std::string noComment = TempMap("cc_map_nocomment.map");
    WriteFile(noComment, "name=N\nauthor=\nwidth=2\nheight=1\n..\n");
    CC_CHECK(!ParseMapFile(noComment, bad));

    // --- apply: resize + terrain + default node amounts ---
    TileMap applied(2, 2);
    ResourceNodes appliedNodes;
    CC_CHECK(ApplyMapData(data, applied, appliedNodes));
    CC_CHECK(applied.Width() == 4 && applied.Height() == 3);
    CC_CHECK(applied.Get({ 2, 0 }) == TerrainType::Water);
    CC_CHECK(applied.Get({ 1, 1 }) == TerrainType::Forest);
    CC_CHECK(applied.Get({ 1, 2 }) == TerrainType::Rock);
    CC_CHECK(applied.Get({ 2, 2 }) == TerrainType::Building);
    CC_CHECK(appliedNodes.Count() == 2);
    const ResourceNode *iron = appliedNodes.FindAt({ 2, 1 });
    CC_CHECK(iron != nullptr && iron->kind == ResourceKind::Iron && iron->amount == 200.0f);

    // --- shipped maps: load, dims, markers, connectivity ---
    const char *shipped[] = { "crossroads.map", "twin_basins.map", "high_ridge.map",
                                "salt_flats.map" };
    for (const char *name : shipped)
    {
        MapData ship;
        CC_CHECK(LoadShipped(name, ship));
        CC_CHECK(ship.width == 24 && ship.height == 18);
        CC_CHECK(ship.playerSpawns.size() == 1 && ship.aiSpawns.size() == 1);
        if (ship.playerSpawns.size() != 1 || ship.aiSpawns.size() != 1)
        {
            continue; // no spawns to test connectivity from (load failed above)
        }
        CC_CHECK(ship.nodes.size() >= 3); // home iron each + contested oil
        TileMap shipMap(4, 4);
        ResourceNodes shipNodes;
        CC_CHECK(ApplyMapData(ship, shipMap, shipNodes));
        const cc::IVec2 home = ship.playerSpawns[0];
        const cc::IVec2 away = ship.aiSpawns[0];
        CC_CHECK(Reachable(shipMap, home, away)); // bases connected
        for (const MapNodeSpawn &spawn : ship.nodes)
        {
            CC_CHECK(!shipMap.IsBlocked(spawn.tile));   // nodes stand on grass
            CC_CHECK(Reachable(shipMap, home, spawn.tile)); // harvesters can walk there
        }
        // Mirror symmetry: every tile mirrors across the center point.
        for (int y = 0; y < ship.height; ++y)
        {
            for (int x = 0; x < ship.width; ++x)
            {
                CC_CHECK(shipMap.Get({ x, y }) ==
                         shipMap.Get({ ship.width - 1 - x, ship.height - 1 - y }));
            }
        }
    }

    // --- 2v2 map: two markers per side, wider dims, same guarantees ---
    {
        MapData falls;
        CC_CHECK(LoadShipped("twin_falls_2v2.map", falls));
        CC_CHECK(falls.width == 28 && falls.height == 20);
        CC_CHECK(falls.playerSpawns.size() == 2 && falls.aiSpawns.size() == 2);
        CC_CHECK(falls.playerSpawns[0] == cc::IVec2(3, 3));
        CC_CHECK(falls.playerSpawns[1] == cc::IVec2(3, 16));
        CC_CHECK(falls.aiSpawns[0] == cc::IVec2(24, 3));
        CC_CHECK(falls.aiSpawns[1] == cc::IVec2(24, 16));
        CC_CHECK(falls.nodes.size() >= 6); // home pairs + contested center
        TileMap fallsMap(4, 4);
        ResourceNodes fallsNodes;
        CC_CHECK(ApplyMapData(falls, fallsMap, fallsNodes));
        for (const cc::IVec2 &home :
             { falls.playerSpawns[0], falls.playerSpawns[1] })
        {
            for (const cc::IVec2 &away : { falls.aiSpawns[0], falls.aiSpawns[1] })
            {
                CC_CHECK(Reachable(fallsMap, home, away)); // every pairing meets
            }
            for (const MapNodeSpawn &spawn : falls.nodes)
            {
                CC_CHECK(!fallsMap.IsBlocked(spawn.tile));
                CC_CHECK(Reachable(fallsMap, home, spawn.tile));
            }
        }
    }

    // --- demo smoke: the exact placements main.cpp uses must fit ---
    {
        MapData demo;
        CC_CHECK(LoadShipped("crossroads.map", demo));
        CC_CHECK(!demo.playerSpawns.empty() && !demo.aiSpawns.empty());
        if (!demo.playerSpawns.empty() && !demo.aiSpawns.empty())
        {
            Registry demoRegistry;
            TileMap demoMap(4, 4);
            ResourceNodes demoNodes;
            ResourceSystem demoResources;
            EventDispatcher demoEvents;
            CC_CHECK(ApplyMapData(demo, demoMap, demoNodes));
            const cc::IVec2 home = demo.playerSpawns[0];
            CC_CHECK(PlaceBuilding(demoRegistry, demoMap, BuildingType::Base, 0, home.x,
                                   home.y) != kInvalidEntity);
            CC_CHECK(PlaceBuilding(demoRegistry, demoMap, BuildingType::ResourceDepot, 0,
                                   home.x + 2, home.y) != kInvalidEntity);
            CC_CHECK(PlaceBuilding(demoRegistry, demoMap, BuildingType::Factory, 0, home.x,
                                   home.y + 2) != kInvalidEntity);
            AICommander demoAI(demoRegistry, demoMap, demoNodes, demoEvents, 1,
                               AIDifficulty::Medium, demo.aiSpawns[0], home);
            demoAI.SetupBase();
            int aiBuildings = 0;
            demoRegistry.Each<Building>([&](Entity, const Building &b) {
                if (b.teamID == 1 && b.state == BuildingState::Operational)
                {
                    ++aiBuildings;
                }
            });
            CC_CHECK(aiBuildings == 3); // AI factory fits the corner too
        }
    }

    // --- save/load roundtrip on a loaded map ---
    MapData cross;
    CC_CHECK(LoadShipped("crossroads.map", cross));
    Registry registry;
    ResourceSystem resources;
    TileMap roundMap(4, 4);
    GameCamera camera;
    ResourceNodes roundNodes;
    FogOfWar fog;
    CC_CHECK(ApplyMapData(cross, roundMap, roundNodes));
    fog.Resize(roundMap.Width(), roundMap.Height());
    WorldState src{ &registry, &resources, &roundMap, &camera, &roundNodes, &fog };
    const std::string savePath = TempMap("cc_map_roundtrip.ccpb");
    CC_CHECK(SaveWorld(src, savePath));
    Registry registry2;
    ResourceSystem resources2;
    TileMap roundMap2(4, 4);
    GameCamera camera2;
    ResourceNodes roundNodes2;
    FogOfWar fog2;
    fog2.Resize(4, 4);
    WorldState dst{ &registry2, &resources2, &roundMap2, &camera2, &roundNodes2, &fog2 };
    CC_CHECK(LoadWorld(dst, savePath));
    CC_CHECK(roundMap2.Width() == 24 && roundMap2.Height() == 18);
    CC_CHECK(roundNodes2.Count() == cross.nodes.size());
    bool terrainMatch = true;
    for (int y = 0; y < 18 && terrainMatch; ++y)
    {
        for (int x = 0; x < 24; ++x)
        {
            if (roundMap2.Get({ x, y }) != roundMap.Get({ x, y }))
            {
                terrainMatch = false;
                break;
            }
        }
    }
    CC_CHECK(terrainMatch);
    std::remove(savePath.c_str());
    std::remove(good.c_str());

    // --- M14: ListMaps enumerates the shipped data/maps dir for setup ---
    const std::string dataDirs[] = { "data/maps", "../../data/maps", "../../../data/maps" };
    std::string dataDir;
    for (const std::string &dir : dataDirs)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(dir, ec) && !ec)
        {
            dataDir = dir;
            break;
        }
    }
    CC_CHECK(!dataDir.empty());
    if (!dataDir.empty())
    {
        const std::vector<MapEntry> maps = ListMaps(dataDir);
        CC_CHECK(maps.size() >= 2); // Crossroads + Twin Basins ship
        bool crossroads = false;
        bool twinBasins = false;
        for (const MapEntry &entry : maps)
        {
            CC_CHECK(!entry.name.empty() && entry.width > 0 && entry.height > 0);
            crossroads = crossroads || entry.name == "Crossroads";
            twinBasins = twinBasins || entry.name == "Twin Basins";
        }
        CC_CHECK(crossroads && twinBasins);
        for (std::size_t i = 1; i < maps.size(); ++i)
        {
            CC_CHECK(maps[i - 1].path <= maps[i].path); // stable setup order
        }
    }
    CC_CHECK(ListMaps(TempMap("cc_no_such_dir_xyz")).empty()); // missing dir

    // --- QoL editor: WriteMapFile round-trips through ParseMapFile ---
    {
        MapData draft;
        draft.name = "Probe";
        draft.author = "test";
        draft.width = 4;
        draft.height = 3;
        draft.terrain.assign(12, TerrainType::Grass);
        draft.terrain[2] = TerrainType::Water;
        draft.terrain[9] = TerrainType::Rock;
        draft.nodes.push_back({ ResourceKind::Iron, { 1, 1 } });
        draft.playerSpawns.push_back({ 0, 0 });
        draft.aiSpawns.push_back({ 3, 2 });
        const std::string outPath = TempMap("cc_editor_roundtrip.map");
        CC_CHECK(WriteMapFile(draft, outPath));
        MapData back;
        CC_CHECK(ParseMapFile(outPath, back));
        CC_CHECK(back.name == "Probe" && back.author == "test");
        CC_CHECK(back.width == 4 && back.height == 3);
        CC_CHECK(back.terrain == draft.terrain);
        CC_CHECK(back.nodes.size() == 1 && back.nodes[0].tile == cc::IVec2(1, 1));
        CC_CHECK(back.playerSpawns.size() == 1 && back.playerSpawns[0] == cc::IVec2(0, 0));
        CC_CHECK(back.aiSpawns.size() == 1 && back.aiSpawns[0] == cc::IVec2(3, 2));
        std::remove(outPath.c_str());
    }

    // --- QoL editor: oversize/empty writes fail without touching disk ---
    {
        MapData huge;
        huge.name = "Huge";
        huge.width = 2048;
        huge.height = 2048;
        huge.terrain.assign(static_cast<std::size_t>(2048) * 2048, TerrainType::Grass);
        const std::string outPath = TempMap("cc_editor_huge.map");
        CC_CHECK(!WriteMapFile(huge, outPath));
        std::error_code ec;
        CC_CHECK(!std::filesystem::exists(outPath, ec));
        MapData noname;
        noname.width = 4;
        noname.height = 4;
        noname.terrain.assign(16, TerrainType::Grass);
        CC_CHECK(!WriteMapFile(noname, TempMap("cc_editor_noname.map")));
    }

    // --- QoL editor: PaintEditorCell marker bookkeeping ---
    {
        MapData canvas;
        canvas.name = "Paint";
        canvas.width = 4;
        canvas.height = 4;
        canvas.terrain.assign(16, TerrainType::Grass);
        PaintEditorCell(canvas, 'I', { 1, 1 });
        PaintEditorCell(canvas, '1', { 0, 0 });
        PaintEditorCell(canvas, '~', { 2, 2 });
        PaintEditorCell(canvas, 'X', { 3, 3 }); // unknown brush: grass, ignored
        PaintEditorCell(canvas, 'I', { 9, 9 }); // out of bounds: ignored
        CC_CHECK(canvas.nodes.size() == 1 && canvas.playerSpawns.size() == 1);
        CC_CHECK(canvas.terrain[1 * 4 + 1] == TerrainType::Grass); // marker sits on grass
        CC_CHECK(canvas.terrain[2 * 4 + 2] == TerrainType::Water);
        // Repainting the node tile with grass clears the marker entry.
        PaintEditorCell(canvas, '.', { 1, 1 });
        CC_CHECK(canvas.nodes.empty());
    }

    // --- QoL editor: ValidateMapPlayable names the first problem ---
    {
        MapData empty;
        empty.name = "Empty";
        empty.width = 8;
        empty.height = 8;
        empty.terrain.assign(64, TerrainType::Grass);
        std::string warning;
        CC_CHECK(!ValidateMapPlayable(empty, &warning)); // no spawns
        CC_CHECK(!warning.empty());
        empty.playerSpawns.push_back({ 0, 0 });
        empty.aiSpawns.push_back({ 7, 7 });
        CC_CHECK(ValidateMapPlayable(empty, nullptr)); // open map connects
        // A rock wall across the middle disconnects the halves.
        for (int x = 0; x < 8; ++x)
        {
            empty.terrain[static_cast<std::size_t>(4) * 8 + x] = TerrainType::Rock;
        }
        CC_CHECK(!ValidateMapPlayable(empty, &warning));
        CC_CHECK(warning.find("connect") != std::string::npos);
    }
}
