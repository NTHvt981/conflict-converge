#pragma once

#include <string>

#include "AICommander.h" // AIDifficulty
#include "MathUtils.h"   // cc::IVec2
#include "raylib.h"      // Vector2

class Registry;
class ResourceSystem;
class TileMap;
class FogOfWar;
class ResourceNodes;
class ProductionQueue;
class UnitFactory;
class GameCamera;

// M14: skirmish world build/teardown shared by main.cpp and headless tests.
// The bundle is non-owning: main.cpp owns the objects as boot-level locals
// and Build/Reset only refill their contents, so shortcut lambdas plus the
// factory/AI reference bindings stay valid across matches.

// Marker-derived start spots (player + AI homes, first iron node).
struct SkirmishSpots
{
    cc::IVec2 playerHome{ 2, 2 };
    cc::IVec2 aiHome{ 16, 9 };
    cc::IVec2 harvest{ 15, 3 };
};

// Parse mapPath for spawn/node markers; missing or unparseable files yield
// the legacy hardcoded defaults.
SkirmishSpots SpotsForMap(const std::string &mapPath);

struct SkirmishWorld
{
    Registry *registry = nullptr;
    ResourceSystem *resources = nullptr;
    TileMap *map = nullptr;
    FogOfWar *fog = nullptr;
    ResourceNodes *nodes = nullptr;
    ProductionQueue *queue = nullptr;
    UnitFactory *factory = nullptr;
    AICommander *ai = nullptr;
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
