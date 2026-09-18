#pragma once

#include <string>

#include "AICommander.h" // AIDifficulty
#include "MathUtils.h"   // cc::IVec2
#include "raylib.h"      // Vector2

class Registry;
class ResourceSystem;
class TileMap;
class OccupancyGrid;
class FogOfWar;
class ResourceNodes;
class ProductionQueue;
class UnitFactory;
class GameCamera;

// Skirmish world build/teardown shared by Game and headless tests.
// The bundle is non-owning: Game owns the objects as members and Build/Reset
// only refills their contents, so shortcut lambdas plus the factory/AI
// reference bindings stay valid across matches.

// Marker-derived start spots (player + AI homes, first iron node).
// 2v2 maps carry two markers per side: allyHome is the second '1', enemyHome2
// the second '2'. is2v2 is true only when both sides field a pair; otherwise
// the extra homes fall back near the primary ones and no extra commander runs.
struct SkirmishSpots
{
    cc::IVec2 playerHome{ 2, 2 };
    cc::IVec2 aiHome{ 16, 9 };
    cc::IVec2 allyHome{ 2, 12 };
    cc::IVec2 enemyHome2{ 16, 5 };
    cc::IVec2 harvest{ 15, 3 };
    bool is2v2 = false;
};

// Parse mapPath for spawn/node markers; missing or unparseable files yield
// the legacy hardcoded defaults.
SkirmishSpots SpotsForMap(const std::string &mapPath);

struct SkirmishWorld
{
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;
    TileMap *map = nullptr;
    OccupancyGrid *occ = nullptr; // Tile occupancy
    FogOfWar *fog = nullptr;
    ResourceNodes *nodes = nullptr;
    ProductionQueue *queue = nullptr;
    UnitFactory *factory = nullptr;
    AICommander *ai = nullptr;
    // 2v2 overflow: allied commander (team 0) + second enemy (team 1).
    // Null in 1v1 builds; BuildSkirmish only touches non-null commanders.
    AICommander *allyAI = nullptr;
    AICommander *enemyAI2 = nullptr;
    GameCamera *camera = nullptr;
    Vector2 *rallyPos = nullptr;
};

// Drop all match content (units, buildings, nodes, queues, AI state). Safe
// on an empty world; map/fog dims are left for Build (or LoadWorld) to set.
void ResetSkirmish(SkirmishWorld &world);

// Reset + seed a full match: starting funds, map terrain + nodes, both
// bases, starting units, factory queue, AI Reset + SetupBase, camera and
// rally aimed at the player home. False only on a null bundle member.
bool BuildSkirmish(SkirmishWorld &world, const std::string &mapPath, AIDifficulty difficulty);

// Reset + seed a prototype sandbox: map terrain only, no funds, no bases,
// no production, no AI — just one player Infantry squad (full squad logic:
// same UnitType, driver, and renderer as every match) at the '1' marker.
// For maps with a player spawn but no AI spawn. False on a null bundle
// member, an unreadable map, or a missing player spawn.
bool BuildSandbox(SkirmishWorld &world, const std::string &mapPath);
