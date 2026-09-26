#pragma once

#include <string>

#include "units/AICommander.h"
#include "core/MathUtils.h"
#include "raylib.h"

class Registry;
class ResourceSystem;
class TileMap;
class OccupancyGrid;
class FogOfWar;
class ResourceNodes;
class ProductionQueue;
class UnitFactory;
class GameCamera;

// Skirmish world build/teardown shared by Game and headless tests. The
// bundle is non-owning: Game owns the objects as members.

// Marker-derived start spots (player + AI homes, first iron node).
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
// the hardcoded defaults.
SkirmishSpots SpotsForMap(const std::string &mapPath);

struct SkirmishWorld
{
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;
    TileMap *map = nullptr;
    OccupancyGrid *occ = nullptr; // tile occupancy
    FogOfWar *fog = nullptr;
    ResourceNodes *nodes = nullptr;
    ProductionQueue *queue = nullptr;
    UnitFactory *factory = nullptr;
    AICommander *ai = nullptr;
    // 2v2 overflow: allied (team 0) + second enemy (team 1). Null in 1v1.
    AICommander *allyAI = nullptr;
    AICommander *enemyAI2 = nullptr;
    GameCamera *camera = nullptr;
    Vector2 *rallyPos = nullptr;
};

// Drop all match content (units, buildings, nodes, queues, AI state).
void ResetSkirmish(SkirmishWorld &world);

// Reset + seed a full match. False only on a null bundle member.
bool BuildSkirmish(SkirmishWorld &world, const std::string &mapPath, AIDifficulty difficulty);

// Reset + seed a prototype sandbox: map terrain + one player RifleInfantry squad,
// no funds/bases/production/AI. False on a null member, unreadable map, or
// missing player spawn.
bool BuildSandbox(SkirmishWorld &world, const std::string &mapPath);
