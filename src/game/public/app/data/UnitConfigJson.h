#pragma once

#define CEREAL_RAPIDJSON_ASSERT(x) \
	do { if (!(x)) throw std::runtime_error("malformed unit-config JSON"); } while (0)

#include "core/MathUtils.h"

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <optional>
#include <stdexcept>
#include <string>

struct JsonStats
{
	std::optional<float> health;
	std::string armorType;
	std::string damageType;
	std::optional<int> attackPower;
	std::optional<cc::TileUnit> attackRange; // tiles
	std::optional<float> cooldownTime;
	std::optional<cc::TileUnit> speed;      // tiles per second
	std::optional<cc::TileUnit> sightRange; // tiles
};

struct JsonBounds
{
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
};

struct JsonOrigin
{
	int x = 0;
	int y = 0;
};

struct JsonCollision
{
	std::optional<JsonBounds> bounds;
	std::optional<JsonOrigin> origin;
};

struct JsonArt
{
	std::string spritePrefix;
	std::string atlasIdlePrefix;
	std::string atlasMovePrefix;
};

struct JsonCost
{
	std::optional<int> iron;
	std::optional<int> oil;
};

struct JsonTurret
{
	std::optional<float> turnRate;
};

struct JsonArcingShell
{
	std::optional<float> flightTime;
	std::optional<int> splashTiles;
};

struct JsonTransport
{
	std::optional<int> capacity;
};

struct JsonAbilities
{
	std::optional<JsonTurret> turret;
	std::optional<JsonArcingShell> arcingShell;
	std::optional<bool> crushesFlesh;
	std::optional<JsonTransport> transport;
	std::optional<bool> sniper;
};

struct JsonConfig
{
	std::string type;
	JsonStats stats;
	JsonCost cost;
	JsonAbilities abilities;
	JsonCollision collision;
	JsonArt art;
};

// Cereal-json bridge between data/configs files and `UnitConfig`. `Load`
// reads every top-level key tolerantly (missing keys keep their default
// members); `Save` emits the hand-editable key order. All serialization
// lives in UnitConfigJson.cpp.
class UnitConfigJson
{
public:
	JsonConfig cfg;

	void Load(cereal::JSONInputArchive &ar);
	void Save(cereal::JSONOutputArchive &ar) const;
};
