# Coding conventions

Rules for writing code in this repository, derived from the code that is here.
Each idea is one subsection with a Do example and a Don't example. When a
change makes a file disagree with a rule, either the file or this document
changes in the same commit.

## Quick reference

One line per area; the linked section carries the Do/Don't pair.

| Area | One-line rule |
| --- | --- |
| [Build](#language-and-build) | C++20, MSVC, x64; regenerate projects after any file add or remove |
| [Layout](#files-and-folders) | Header in `public/<module>/`, TU in `private/<module>/`, PascalCase file names |
| [Naming](#naming) | PascalCase types/functions; camelCase data; `_` class members; `k` constants; `Count` last |
| [Includes](#headers-and-includes) | `#pragma once`; own header first; root-qualified includes; forward-declare cycles |
| [Format](#formatting) | Tabs; Allman braces; `const T &`; comma-first init lists; 100 columns; LF |
| [Structure](#structure) | Data structs with defaults; free-function systems; `Subsystem` scopes, no globals |
| [Errors](#error-handling) | `CC_ASSERT` invariants; bool/sentinel failures; throw only while parsing |
| [Comments](#comments) | Contract, not syntax; present tense; units on meaningful numbers |
| [Data](#data-saves-and-assets) | camelCase JSON; append-only save wire; version and magic bump on a break |
| [Tests](#tests) | `CC_CHECK`; suite registered in `tests/main.cpp`; headless; Debug and Release |
| [Workflow](#docs-and-change-workflow) | Feature doc and index; prefixed branch; full gate before a pull request |
| [Pull request body](#keep-the-pull-request-body-to-what-the-diff-cannot-show) | Why, behavior change, proof, merge order; the diff carries the rest |

## Language and build

### Use C++20, MSVC, and x64 only

**✅ Do**
```lua
platforms { "x64" }
```

**❌ Don't**
```lua
platforms { "x64", "x32" } -- 32-bit hits the 2 GB limit and weakens the optimizer
```

### Regenerate projects after adding or removing a file

**✅ Do**
```
add src/game/private/units/UnitPatrol.cpp
premake5 vs2022
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
```

**❌ Don't**
```
add src/game/private/units/UnitPatrol.cpp
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
```

The file is not in the generated project yet, so its symbols stay unresolved
until the next regeneration.

### Consume dependencies as vendored static libraries

**✅ Do**
```cpp
#include <glm/glm.hpp>
#include <cereal/cereal.hpp>
#include "raylib.h"
```

**❌ Don't**
```cpp
#include "../../deps/glm/glm/glm.hpp"
#include <SFML/Graphics.hpp> // not in libs_deps.json
```

### Include from the two game roots only

**✅ Do**
```cpp
#include "units/Unit.h"
#include "core/Game.h"
```

**❌ Don't**
```cpp
#include "../units/Unit.h"
#include "../../src/game/public/units/Unit.h"
```

### Keep shared TUs free of main, window, and menu flow

**✅ Do**
```cpp
void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds);
```

**❌ Don't**
```cpp
void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds)
{
	DrawHud(); // pulls the renderer into headless suites
}
```

## Files and folders

### Keep one header per main type, named after it

**✅ Do**
```
src/game/public/units/Unit.h
src/game/public/units/AICommander.h
src/game/public/app/save/SaveWire.h // wire schema: SaveVec2, SaveUnit, ...
```

**❌ Don't**
```
src/game/public/units/unit.h
src/game/public/units/Units.h
src/game/public/units/Misc.h
```

### Headers in public, implementation in private, paths mirrored

**✅ Do**
```
src/game/public/units/Unit.h
src/game/private/units/Unit.cpp
src/game/public/units/UnitInternal.h
```

**❌ Don't**
```
src/game/private/units/Unit.h
src/game/Unit.cpp
src/game/public/units/Unit.cpp
```

Internal headers such as `UnitInternal.h`, `Extensions.h`, and `SaveWire.h`
live in `public/` too, so other modules and tests can include them.

### Mirror the app subfolders

**✅ Do**
```
src/game/public/app/ui/Hud.h
src/game/private/app/ui/Hud.cpp
src/game/public/app/input/Hotkeys.h
src/game/private/app/input/Hotkeys.cpp
```

**❌ Don't**
```
src/game/private/app/Hud.cpp
src/game/public/app/Hotkeys.h
```

### Split an oversized TU by phase, keep one header

**✅ Do**
```
src/game/public/core/GameRenderer.h
src/game/private/core/GameRendererWorld.cpp
src/game/private/core/GameRendererHud.cpp
```

**❌ Don't**
```
src/game/private/core/GameRenderer.cpp   // 3000 lines mixing world and HUD
src/game/public/core/GameRendererWorld.h // header split nobody includes
```

### Never name a header like a CRT header

**✅ Do**
```cpp
#include "core/MathUtils.h"
#include "core/CcAssert.h"
```

**❌ Don't**
```cpp
#include "core/Math.h"   // shadows <math.h> on Windows
#include "core/Assert.h" // shadows <assert.h>
```

### Keep third-party implementations out of game folders

**✅ Do**
```
src/thirdparty/raygui_impl.c
src/thirdparty/rini_impl.cpp
```

**❌ Don't**
```
src/game/private/app/ui/raygui_impl.c
src/game/public/rini_impl.cpp
```

### Put tests and tools where the projects expect them

**✅ Do**
```
tests/unit-tests/hud_tests.cpp
tests/integration-tests/integration_tests.cpp
tests/e2e/e2e_main.cpp
tools/unit_editor/
```

**❌ Don't**
```
src/game/private/app/ui/HudTests.cpp
tests/hud_test.cpp
```

## Naming

### Use PascalCase for types

**✅ Do**
```cpp
struct QueuedOrder { };
enum class AIDifficulty { Easy, Medium, Hard };
class RmlUiHost : public Subsystem { };
```

**❌ Don't**
```cpp
struct queued_order { };
enum class ai_difficulty { EASY, HARD };
class rmluiHost { };
```

### Use PascalCase, verb-first function names

**✅ Do**
```cpp
void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 worldTarget);
void UpdateUnitMovement(Unit &unit, Orders &orders, Mover &mover);
```

**❌ Don't**
```cpp
void move_order(Unit &unit);
void orderIssue(Unit &unit);
```

### Name predicates as questions

**✅ Do**
```cpp
bool IsBlocked(cc::IVec2 tile) const;
bool CanEnter(const TileMap &map, cc::IVec2 tile, int w, int h) const;
template <typename T> bool Has(Entity entity) const;
bool ShouldPlay(SfxId id, double now);
```

**❌ Don't**
```cpp
bool CheckBlocked(cc::IVec2 tile) const;
bool Blocked(cc::IVec2 tile) const;
bool Enter(cc::IVec2 tile) const;
```

### Use camelCase for variables, parameters, and struct fields

**✅ Do**
```cpp
struct Unit
{
	int footprintWidth = 1;
	Vector2 position = {};
};

void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 worldTarget);
```

**❌ Don't**
```cpp
struct Unit
{
	int FootprintWidth = 1;
	Vector2 Position = {};
};

void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 WorldTarget);
```

### Mark class data with a trailing underscore

**✅ Do**
```cpp
class Game
{
	EngineScope engine_;
	WorldScope world_;
	std::function<void()> quitToMenu_;
};
```

**❌ Don't**
```cpp
class Game
{
	EngineScope m_engine;
	WorldScope world;
	std::function<void()> QuitToMenu;
};
```

Struct fields stay plain camelCase: `Unit::health`, `AIDifficultyParams::waveThreshold`.

### Name constants with k and keep them constexpr

**✅ Do**
```cpp
inline constexpr Entity kInvalidEntity = 0;
inline constexpr unsigned int kSaveVersion = 3;
inline constexpr float kRetreatHealthFraction = 0.3f;
```

**❌ Don't**
```cpp
#define RETREAT 0.3
const float RETREAT_HEALTH = 0.3f;
static int saveVersion = 3;
```

`cc::TILE_SIZE` is the all-caps exception; leave it that way.

### Use cc, save_detail, and anonymous namespaces

**✅ Do**
```cpp
namespace cc
{
Vec2 ToGlm(Vector2 v);
} // namespace cc

namespace
{
StepResult StepToward(cc::Vec2 pos, cc::Vec2 target, float step);
} // namespace
```

**❌ Don't**
```cpp
using namespace std;

namespace utilities
{
static StepResult StepToward(cc::Vec2 pos, cc::Vec2 target, float step);
}
```

### Give enums a Count sentinel, and spell enumerators in PascalCase

**✅ Do**
```cpp
enum class UnitType
{
	RifleInfantry,
	AntiArmorInfantry,
	Engineer,
	Count
};
```

**❌ Don't**
```cpp
enum class UnitType
{
	RIFLE_INFANTRY,
	ANTI_ARMOR_INFANTRY,
	ENGINEER,
	LAST
};
```

Enumerators that are data spellings stay uppercase (`DamageType::KINETIC`,
`ArmorType::STEEL`).

### Append enum values, never insert them

**✅ Do**
```cpp
enum class TerrainType
{
	Grass,
	Water,
	Building,
	Forest,
	Rock,
	Count
};
```

**❌ Don't**
```cpp
enum class TerrainType
{
	Grass,
	Forest, // shifts Water/Building/Rock and breaks every save
	Water,
	Building,
	Rock,
	Count
};
```

### Pass a generation whenever an entity id outlives the call

**✅ Do**
```cpp
occ.ReserveFootprint(at, fp.x, fp.y, entity, registry.Generation(entity));
```

**❌ Don't**
```cpp
occ.ReserveFootprint(at, fp.x, fp.y, entity, 0); // a recycled id now owns this tile
```

### Read booleans as predicates, options as behavior names

**✅ Do**
```cpp
bool isSelected = false;
bool showHints = true;
void IssueOrEnqueue(..., bool shiftQueue, QueuedOrder order);
```

**❌ Don't**
```cpp
bool flag = false;
bool mode = true;
void IssueOrEnqueue(..., bool b, QueuedOrder order);
```

## Headers and includes

### Use pragma once

**✅ Do**
```cpp
#pragma once

#include <vector>
```

**❌ Don't**
```cpp
#ifndef UNIT_H
#define UNIT_H
#include <vector>
#endif
```

### Include your own header first in a TU

**✅ Do**
```cpp
#include "units/Unit.h"

#include <algorithm>
#include <cmath>

#include "core/MathUtils.h"
#include "world/Pathfinder.h"
```

**❌ Don't**
```cpp
#include <algorithm>
#include <cmath>

#include "units/Unit.h"
#include "core/MathUtils.h"
```

### Qualify project includes from the include root

**✅ Do**
```cpp
#include "units/Unit.h"
#include "world/TileMap.h"
#include "app/ui/RmlUiHost.h"
```

**❌ Don't**
```cpp
#include "Unit.h"
#include "TileMap.h"
#include "RmlUiHost.h"
```

### Never comment on an include line

**✅ Do**
```cpp
#include "units/Unit.h"
#include "units/Extensions.h"
```

**❌ Don't**
```cpp
#include "units/Unit.h"   // the unit component
#include "units/Extensions.h" // orders and movers
```

### Forward-declare to break cycles

**✅ Do**
```cpp
// Unit.h
class TileMap;
class OccupancyGrid;
struct Orders;
struct Mover;
```

**❌ Don't**
```cpp
// Unit.h
#include "world/Pathfinder.h" // Pathfinder.h includes Unit.h
```

### Include what you use

**✅ Do**
```cpp
#include <cstdint> // std::uint32_t
#include <cstddef> // std::size_t
#include <vector>
```

**❌ Don't**
```cpp
// relies on raylib.h or another header to bring in std::uint32_t
```

### Keep raylib types at the boundary, cc types inside

**✅ Do**
```cpp
inline void SnapUnitToTile(Unit &unit)
{
	unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}
```

**❌ Don't**
```cpp
cc::Vec2 mixed = unit.position; // raylib Vector2 into glm without a conversion
```

## Formatting

### Indent with tabs

**✅ Do**
```cpp
void UpdateUnit(Entity self, Registry &registry, float dtSeconds)
{
	if (!registry.IsAlive(self))
	{
		return;
	}
}
```

**❌ Don't**
```cpp
void UpdateUnit(Entity self, Registry &registry, float dtSeconds)
{
    if (!registry.IsAlive(self))
    {
        return;
    }
}
```

### Put braces on their own line (Allman)

**✅ Do**
```cpp
if (map.IsBlocked(tile))
{
	return false;
}
else
{
	return true;
}
```

**❌ Don't**
```cpp
if (map.IsBlocked(tile)) {
	return false;
} else {
	return true;
}
```

### Allow a one-line early exit, brace everything else

**✅ Do**
```cpp
if (!open_)
	return;

if (map.IsBlocked(tile))
{
	return false;
}
```

**❌ Don't**
```cpp
if (!map.IsBlocked(tile)) unit.position = target;

if (!open_) { open_ = true; return; }
```

### Declare one thing per line

**✅ Do**
```cpp
int width = 0;
int height = 0;
template <typename T> void Add(Entity entity, T component);
template <typename T> bool Has(Entity entity) const;
```

**❌ Don't**
```cpp
int width = 0, height = 0;
template <typename T> void Add(Entity entity, T component); template <typename T> bool Has(Entity entity) const;
```

### Bind pointers and references to the name

**✅ Do**
```cpp
float EffectiveSpeed(const Unit &unit, const Mover &mover);
void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ);
```

**❌ Don't**
```cpp
float EffectiveSpeed(const Unit& unit, const Mover& mover);
void ResolveStackedUnits(Registry& registry, const TileMap& map, OccupancyGrid& occ);
```

### Follow the spacing rules

**✅ Do**
```cpp
if (map.IsBlocked(tile))
{
	for (int i = 0; i < count; ++i)
	{
		total += i;
	}
}
```

**❌ Don't**
```cpp
if(map.IsBlocked(tile)){
	for(int i=0;i<count;++i){
		total+=i;
	}
}
```

### Put one initializer per line, comma first

**✅ Do**
```cpp
RmlUiHud::RmlUiHud(Registry &registry, ResourceSystem &resources)
	: registry_(registry)
	, resources_(resources)
{
}
```

**❌ Don't**
```cpp
RmlUiHud::RmlUiHud(Registry &registry, ResourceSystem &resources)
	: registry_(registry), resources_(resources)
{
}
```

### Stay within 100 columns and align wrapped arguments

**✅ Do**
```cpp
void IssueAttackMoveOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
						  Vector2 worldTarget);
```

**❌ Don't**
```cpp
void IssueAttackMoveOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map, OccupancyGrid &occ, Entity self, std::uint32_t selfGen, Vector2 worldTarget);
```

### Use float literals with f, int for counts, fixed width on the wire

**✅ Do**
```cpp
float speed = 96.0f;
float health = 100.0f;
int tileCount = 400;
std::uint32_t selfGen = 0;
```

**❌ Don't**
```cpp
float speed = 96;
float health = 100.0; // double narrowed to float
long tileCount = 400;
unsigned int selfGen = 0;
```

### End every file with LF and no trailing whitespace

**✅ Do**
```
file: UTF-8 Unicode text
last byte: 0x0a
```

**❌ Don't**
```
file: ASCII text, with CRLF line terminators
last line: "...   "   (trailing spaces)
```

## Structure

### Give each header one concept

**✅ Do**
```cpp
// Hud.h: HUD text builders only.
const char *UnitTypeName(UnitType type);
std::string SelectionSummary(const Unit &unit);
```

**❌ Don't**
```cpp
// Hud.h
const char *UnitTypeName(UnitType type);
class SaveGame { };        // unrelated
struct ParticleSystem { }; // unrelated
```

### Give component structs in-class defaults

**✅ Do**
```cpp
struct Unit
{
	float health = 100.0f;
	Vector2 position = {};
	bool isSelected = false;
};
```

**❌ Don't**
```cpp
struct Unit
{
	float health;
	Vector2 position;
	bool isSelected;
};
```

### Write systems as free functions that take what they mutate first

**✅ Do**
```cpp
void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds);
void ClearOrders(Unit &unit, Orders &orders, Mover &mover);
```

**❌ Don't**
```cpp
class Unit
{
public:
	void Update(Registry &registry, TileMap &map, float dtSeconds);
	void ClearOrders(Orders &orders, Mover &mover);
};
```

### Own services in a Subsystem scope, never globally

**✅ Do**
```cpp
class Audio : public Subsystem
{
public:
	bool Init(bool withDevice);
	void Shutdown();
	bool ShouldPlay(SfxId id, double now);
};
```

**❌ Don't**
```cpp
static Audio &AudioInstance()
{
	static Audio audio;
	return audio;
}
```

### Require dependencies by reference, make optional ones pointers

**✅ Do**
```cpp
void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
						  const FogOfWar *fog, float dtSeconds);
```

**❌ Don't**
```cpp
void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
						  const FogOfWar &fog, float dtSeconds); // headless has no fog
```

### Apply const where it is true

**✅ Do**
```cpp
bool IsAlive(Entity entity) const;
std::size_t EntityCount() const;
void ResolveCrush(Registry &registry);
```

**❌ Don't**
```cpp
bool IsAlive(Entity entity);
std::size_t EntityCount();
void ResolveCrush(const Registry &registry);
```

### Delete copy and move on owning services

**✅ Do**
```cpp
Subsystems(const Subsystems &) = delete;
Subsystems &operator=(const Subsystems &) = delete;
Subsystems(Subsystems &&) = delete;
```

**❌ Don't**
```cpp
Subsystems(const Subsystems &) = default; // two owners of the same pools
```

### Do not add a module cycle

**✅ Do**
```cpp
// Pathfinder.h may include units/Unit.h for UnitType.
#include "units/Unit.h"
```

**❌ Don't**
```cpp
// Unit.h must not include Pathfinder.h back.
#include "world/Pathfinder.h"
```

### Put per-frame work in the owner of that phase

**✅ Do**
```cpp
// Simulation.cpp
void Simulation::Step(float dtSeconds)
{
	UpdateUnit(self, registry, map, dtSeconds);
}
```

**❌ Don't**
```cpp
// main.cpp
int main(void)
{
	Game game;
	game.UpdateParticles(); // bypasses Simulation and the tests
	return game.Run();
}
```

## Error handling

### Use CC_ASSERT for programmer invariants, without side effects

**✅ Do**
```cpp
void TileMap::Set(cc::IVec2 tile, TerrainType terrain)
{
	CC_ASSERT(InBounds(tile));
	tiles_[Index(tile)] = terrain;
}
```

**❌ Don't**
```cpp
CC_ASSERT(TrySpawnWave()); // the spawn disappears under NDEBUG
```

### Return failure for expected errors, with a Can check beside the action

**✅ Do**
```cpp
bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target);
void IssueRepairOrder(Unit &engineer, Orders &orders, Mover &mover, Entity target);
```

**❌ Don't**
```cpp
void IssueRepairOrder(Unit &engineer, Entity target)
{
	if (!CanRepairTarget(registry, engineer, target))
	{
		throw std::invalid_argument("bad target"); // gameplay never throws
	}
}
```

### Throw only in data parsing, catch at the load boundary

**✅ Do**
```cpp
#define CEREAL_RAPIDJSON_ASSERT(x) \
	do { if (!(x)) throw std::runtime_error("malformed unit-config JSON"); } while (0)

try
{
	config.loadFrom(file);
}
catch (const std::exception &)
{
	return false;
}
```

**❌ Don't**
```cpp
void UpdateUnit(Entity self, Registry &registry, float dtSeconds)
{
	if (!registry.IsAlive(self))
	{
		throw std::runtime_error("dead entity");
	}
}
```

### Route diagnostics through Log

**✅ Do**
```cpp
Log::Fatal("RmlUi menu documents failed to load from data/ui");
Log::Init("data/logs/conflict-converge.log");
```

**❌ Don't**
```cpp
std::cout << "menu load failed" << std::endl;
printf("menu load failed\n");
```

### Validate before you apply

**✅ Do**
```cpp
char magic[sizeof(kMagic)] = {};
if (!file.read(magic, sizeof(magic)) || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0)
{
	return false;
}
if (version > kSaveVersion || version < kMinSupportedVersion)
{
	return false;
}
// decode into the live state only after the checks pass
```

**❌ Don't**
```cpp
registry.Clear(); // wipes the match before the version check fails
if (version > kSaveVersion)
{
	return false;
}
```

## Comments

### Comment the contract, not the syntax

**✅ Do**
```cpp
float attackRange = 0; // pixels
float cooldownTime = 0; // seconds between attacks

// Empty when unreachable, either end blocked, or the map has no tiles.
TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal);
```

**❌ Don't**
```cpp
float attackRange = 0; // attack range
// This function finds a path.
TilePath FindPath(const TileMap &map, cc::IVec2 start, cc::IVec2 goal);
```

### Write live comments in present tense

**✅ Do**
```cpp
// Returns the nearest free tile; the caller spawns there.
cc::IVec2 NearestFreeTile(const TileMap &map, cc::IVec2 want);
```

**❌ Don't**
```cpp
// Will return the nearest free tile once the spawn rework lands.
cc::IVec2 NearestFreeTile(const TileMap &map, cc::IVec2 want);
```

### Name meaningful numbers or state their unit

**✅ Do**
```cpp
inline constexpr int kReplayMaxFrames = 300; // 10 min at the 2s cadence
inline constexpr float kRetreatHealthFraction = 0.3f;
```

**❌ Don't**
```cpp
if (replayFrame > 300)
{
	Retreat(0.3f);
}
```

## Data, saves, and assets

### Use camelCase JSON keys that mirror the C++ members

**✅ Do**
```json
{
	"stats": {
		"attackPower": 20,
		"sightRange": 320,
		"armorType": "STEEL"
	}
}
```

**❌ Don't**
```json
{
	"stats": {
		"attack_power": 20,
		"sight_range": 320,
		"armor_type": "STEEL"
	}
}
```

### Default missing data, reject malformed data

**✅ Do**
```cpp
float sightRange = 0.0f; // key absent -> keep the default
// A present but malformed value throws and rejects the whole file.
```

**❌ Don't**
```cpp
if (!json.contains("sightRange"))
{
	throw std::runtime_error("missing sightRange"); // breaks every old config
}
```

### Keep the save wire append-only until a version bump

**✅ Do**
```cpp
ar(cereal::make_nvp("iron", iron),
   cereal::make_nvp("oil", oil),
   cereal::make_nvp("newField", newField)); // appended with a default
```

**❌ Don't**
```cpp
ar(cereal::make_nvp("metal", iron), // renamed: every old save loses iron
   cereal::make_nvp("oil", oil));
```

A break bumps `kSaveVersion` and the magic (`CCJ3`) together.

### Guard enum growth with a save-decode static_assert

**✅ Do**
```cpp
static_assert(static_cast<int>(UnitType::Count) == 9, "UnitType grew: review save decode");
```

**❌ Don't**
```cpp
enum class UnitType { RifleInfantry, Medic, Count }; // Decode not reviewed
```

### Indent RML and RCSS with tabs, one per nesting level

**✅ Do**
```html
<body id="hud-body">
	<div class="hud-panel" id="hud-stockpile">
		<div class="hud-title">Stockpile</div>
		<div class="hud-line" id="res-line">Iron: 500  Oil: 250</div>
	</div>
</body>
```

**❌ Don't**
```html
<body id="hud-body">
    <div class="hud-panel" id="hud-stockpile">
        <div class="hud-title">Stockpile</div>
    </div>
</body>
```

### Let the map loader place spawns on valid tiles

**✅ Do**
```cpp
const cc::IVec2 spawn = NearestFreeTile(map, markerTile.x, markerTile.y);
factory.Spawn(type, teamID, cc::ToRaylib(cc::TileToWorld(spawn.x, spawn.y)));
```

**❌ Don't**
```cpp
factory.Spawn(type, teamID, cc::ToRaylib(cc::TileToWorld(markerTile.x, markerTile.y)));
// the marker can sit inside a Building footprint; the unit spawns trapped
```

## Tests

### Give each subsystem a suite file and register it

**✅ Do**
```cpp
// tests/unit-tests/fog_tests.cpp
void RunFogTests()
{
	// ...
}

// tests/main.cpp
void RunFogTests();
RunFogTests();
```

**❌ Don't**
```cpp
// tests/unit-tests/fog_tests.cpp
// never declared or called from tests/main.cpp
```

### Check behavior with CC_CHECK

**✅ Do**
```cpp
CC_CHECK(FormatResources(resources) == "Iron: 380  Oil: 310");
```

**❌ Don't**
```cpp
assert(FormatResources(resources) == "Iron: 380  Oil: 310"); // gone in Release
```

### Keep unit tests headless

**✅ Do**
```cpp
CC_CHECK(SelectionSummary(unit) == "Heavy Tank  HP 320/500  Team 1");
```

**❌ Don't**
```cpp
InitWindow(800, 450, "test"); // no GPU on CI, and the suite must not open windows
```

Extract the logic into a pure function when it lives in a rendered path
(`SelectionSummary`, `FormatResources`, HUD text builders).

### Keep perf checks step-bounded with a loose budget

**✅ Do**
```cpp
constexpr float kDt = 1.0f / 60.0f;
constexpr int kFrames = 600;
const auto start = std::chrono::steady_clock::now();
for (int f = 0; f < kFrames; ++f)
{
	UpdateUnit(id, registry, map, kDt);
}
const double ms = std::chrono::duration<double, std::milli>(
	std::chrono::steady_clock::now() - start).count();
CC_CHECK(ms < 5000.0); // ~8x the observed Debug cost; catches runaway work
```

**❌ Don't**
```cpp
CC_CHECK(ms < 16.7); // ties CI to one machine and one load
```

### Run Debug and Release

**✅ Do**
```
prj/bin/Debug/conflict-converge-test.exe
prj/bin/Release/conflict-converge-test.exe
```

**❌ Don't**
```
prj/bin/Debug/conflict-converge-test.exe
# stop there: NDEBUG-only paths and the assert-compiled test go untested
```

## Docs and change workflow

### Document each feature once and index it

**✅ Do**
```
docs/features/orders-and-queueing.md
docs/features/README.md -> "- [orders-and-queueing](orders-and-queueing.md) - ..."
```

**❌ Don't**
```
src/game/private/units/UnitOrders.cpp  // the only description of the feature
```

### Describe current behavior in live docs

**✅ Do**
```
Guard units chase targets inside sight range and return to the rally point.
```

**❌ Don't**
```
- M14: reworked retreat so units fall back at 30% HP.
```

History belongs in `docs/history/`, `docs/PROJECT_NOTES.md`, or `plans/`.

### Follow the ASCII diagram convention

**✅ Do**
```
┌─ client ────┐      ┌─ queue ─────┐
│ POST /jobs  │─────▶│ jobs.build  │
└─────────────┘      └─────────────┘
```

**❌ Don't**
```
🚀 client ==> QUEUE ==> worker (a 120-column line with emoji, no legend)
```

Bare fence, at most 70 columns, one box width, legend for every non-edge
symbol. See `docs/conventions/ascii-diagram.md`.

### Use prefixed branches and imperative commit subjects

**✅ Do**
```
branch: fix/hud-building-selection-line
commit: Split the selection line into one builder per case
```

**❌ Don't**
```
branch: stuff
commit: feat: fixed some hud things
```

### Keep the pull request body to what the diff cannot show

**✅ Do**
```
Descriptor tables replace fourteen per-type switches: every UnitType /
BuildingType fact is one row, read only through InfoOf().

Behavior change: DefaultUnitConfig(PrototypeInfantry).spritePrefix now
follows the art value.

Verified: Debug 4942 checks / 0 failures, Release 4926 / 0. The one e2e
failure also fails at HEAD.

Stacked on docs/coding-conventions: merge that first.
```

**❌ Don't**
```
## Files changed
- UnitTypeInfo.h: adds UnitTypeInfo, kUnitTypes, InfoOf, a static_assert
- Hud.cpp: replaces the UnitTypeName switch with InfoOf
## How the table works
Each row holds one type's fields; the static_assert ties the row count to
UnitType::Count, so a new enumerator fails to compile.
```

A reviewer reads the diff for what changed and how. The body carries only why,
behavior changes, proof, and merge order — plus the one or two decisions a
reader would otherwise have to ask about (a deliberate oddity, a pre-existing
failure, a stacked base).

### Run the full gate before a pull request

**✅ Do**
```
premake5 vs2022
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=x64
prj/bin/Debug/conflict-converge-test.exe
prj/bin/Release/conflict-converge-test.exe
prj/bin/Debug/conflict-converge-e2e.exe
```

**❌ Don't**
```
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
# open the PR with one config built and no test run
```

## Checklist

- [ ] x64 + C++20, projects regenerated after file changes
- [ ] header in `public/`, TU in `private/`, `#pragma once`, no new cycle
- [ ] PascalCase types/functions, camelCase data, `_` class members, `k` constants
- [ ] tabs, Allman braces, `const T &`, `= {}` defaults, LF, 100 columns
- [ ] `CC_ASSERT` invariants, bool/sentinel failures, no throwing gameplay
- [ ] JSON camelCase, save wire append-only, version bumped on a break
- [ ] `CC_CHECK` covers the behavior and is registered in `tests/main.cpp`
- [ ] Debug and Release green, e2e run when the loop or UI changed
- [ ] feature doc and index updated, present tense, diagrams per ASCII convention
