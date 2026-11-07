
#include "app/data/UnitConfig.h"

#include "UnitConfigJson.h"

#include <cctype>
#include <ranges>
#include <sstream>
#include <string>

#include "units/UnitTypeInfo.h"

namespace
{

std::string NormalizedStem(std::string s)
{
	std::string out;
	for (char &c : s)
	{
		if (c == '_')
		{
			continue;
		}
		out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return out;
}

// Fail fast on typos: unknown keys inside "abilities" reject the file.
// Tolerant everywhere else (missing keys default-fill per plan).
bool HasUnknownAbilityKey(const std::string &json)
{
	static const char *kKnown[] = {
		"turret",       "turnRate",   "turn_rate",   "arcingShell", "arcing_shell",
		"flightTime",   "flight_time", "splashTiles", "splash_tiles", "crushesFlesh",
		"crushes_flesh", "transport",  "capacity",    "sniper",      "shotsPerSprite",
		"shots_per_sprite",
	};
	const std::size_t pos = json.find("\"abilities\"");
	if (pos == std::string::npos)
	{
		return false;
	}
	const std::size_t open = json.find('{', pos);
	if (open == std::string::npos)
	{
		return false;
	}
	int depth = 0;
	std::size_t close = std::string::npos;
	for (std::size_t i = open; i < json.size(); ++i)
	{
		if (json[i] == '{')
		{
			++depth;
		}
		else if (json[i] == '}')
		{
			if (--depth == 0)
			{
				close = i;
				break;
			}
		}
	}
	if (close == std::string::npos)
	{
		return false;
	}
	const std::string block = json.substr(open, close - open + 1);
	std::size_t i = 0;
	while (i < block.size())
	{
		if (block[i] != '"')
		{
			++i;
			continue;
		}
		const std::size_t end = block.find('"', i + 1);
		if (end == std::string::npos)
		{
			break;
		}
		const std::string key = block.substr(i + 1, end - i - 1);
		std::size_t j = end + 1;
		while (j < block.size() && (block[j] == ' ' || block[j] == '\t' ||
									block[j] == '\n' || block[j] == '\r'))
		{
			++j;
		}
		if (j < block.size() && block[j] == ':')
		{
			bool known = false;
			for (const char *k : kKnown)
			{
				if (key == k)
				{
					known = true;
					break;
				}
			}
			if (!known)
			{
				return true;
			}
		}
		i = end + 1;
	}
	return false;
}

}

bool IsUnitConfigStem(const std::string &stem)
{
	const std::string normalized = NormalizedStem(stem);
	for (const UnitTypeInfo &info : kUnitTypes)
	{
		if (NormalizedStem(info.fileStem) == normalized)
		{
			return true;
		}
	}
	return false;
}

std::expected<UnitType, ConfigError> ParseUnitTypeName(const std::string &name)
{
	if (name == "Infantry") // "Infantry" alias
	{
		return UnitType::RifleInfantry;
	}
	for (const auto &[i, info] : kUnitTypes | std::views::enumerate)
	{
		if (name == info.configName)
		{
			return static_cast<UnitType>(i);
		}
	}
	return std::unexpected(ConfigError::UnknownName);
}

std::expected<ArmorType, ConfigError> ParseArmorTypeName(const std::string &name)
{
	if (name == "STEEL")
	{
		return ArmorType::STEEL;
	}
	if (name == "RUBBER")
	{
		return ArmorType::RUBBER;
	}
	if (name == "COMPOSITE")
	{
		return ArmorType::COMPOSITE;
	}
	return std::unexpected(ConfigError::UnknownName);
}

std::expected<DamageType, ConfigError> ParseDamageTypeName(const std::string &name)
{
	if (name == "KINETIC")
	{
		return DamageType::KINETIC;
	}
	if (name == "EXPLOSIVE")
	{
		return DamageType::EXPLOSIVE;
	}
	if (name == "ENERGY")
	{
		return DamageType::ENERGY;
	}
	return std::unexpected(ConfigError::UnknownName);
}

const char *ArmorTypeName(ArmorType type)
{
	switch (type)
	{
	case ArmorType::STEEL:
		return "STEEL";
	case ArmorType::RUBBER:
		return "RUBBER";
	case ArmorType::COMPOSITE:
		return "COMPOSITE";
	case ArmorType::Count:
		break;
	}
	return "";
}

const char *DamageTypeName(DamageType type)
{
	switch (type)
	{
	case DamageType::KINETIC:
		return "KINETIC";
	case DamageType::EXPLOSIVE:
		return "EXPLOSIVE";
	case DamageType::ENERGY:
		return "ENERGY";
	case DamageType::Count:
		break;
	}
	return "";
}

std::expected<UnitConfig, ConfigError>
ParseUnitConfigJson(const std::string &json, const std::string &filenameStem)
{
	if (HasUnknownAbilityKey(json))
	{
		return std::unexpected(ConfigError::UnknownAbilityKey);
	}
	JsonConfig cfg;
	try
	{
		std::istringstream in(json);
		cereal::JSONInputArchive ar(in);
		TryLoadValue(ar, "type", cfg.type);
		TryLoadValue(ar, "stats", cfg.stats);
		TryLoadValue(ar, "cost", cfg.cost);
		TryLoadValue(ar, "abilities", cfg.abilities);
		TryLoadValue(ar, "collision", cfg.collision);
		TryLoadValue(ar, "art", cfg.art);
	}
	catch (const std::exception &)
	{
		return std::unexpected(ConfigError::MalformedJson);
	}
	const std::expected<UnitType, ConfigError> type = ParseUnitTypeName(cfg.type);
	if (!type)
	{
		return std::unexpected(type.error());
	}
	if (NormalizedStem(cfg.type) != NormalizedStem(filenameStem))
	{
		return std::unexpected(ConfigError::Mismatch);
	}

	UnitConfig config = DefaultUnitConfig(*type);
	const JsonStats &stats = cfg.stats;
	if (stats.health.has_value() && *stats.health > 0.0f)
	{
		config.stats.health = *stats.health;
	}
	const std::expected<ArmorType, ConfigError> armor = ParseArmorTypeName(stats.armorType);
	if (!stats.armorType.empty() && armor)
	{
		config.stats.armorType = *armor;
	}
	const std::expected<DamageType, ConfigError> damage = ParseDamageTypeName(stats.damageType);
	if (!stats.damageType.empty() && damage)
	{
		config.stats.damageType = *damage;
	}
	if (stats.attackPower.has_value())
	{
		config.stats.attackPower = *stats.attackPower;
	}
	if (stats.attackRange.has_value())
	{
		config.stats.attackRange = *stats.attackRange;
	}
	if (stats.cooldownTime.has_value())
	{
		config.stats.cooldownTime = *stats.cooldownTime;
	}
	if (stats.speed.has_value())
	{
		config.stats.speed = *stats.speed;
	}
	if (stats.sightRange.has_value())
	{
		config.stats.sightRange = *stats.sightRange;
	}
	if (cfg.cost.iron.has_value() && *cfg.cost.iron >= 0)
	{
		config.costIron = *cfg.cost.iron;
	}
	if (cfg.cost.oil.has_value() && *cfg.cost.oil >= 0)
	{
		config.costOil = *cfg.cost.oil;
	}
	if (cfg.abilities.turret.has_value() && cfg.abilities.turret->turnRate.has_value() &&
		*cfg.abilities.turret->turnRate > 0.0f)
	{
		config.abilities.turretTurnRate = *cfg.abilities.turret->turnRate;
	}
	if (cfg.abilities.arcingShell.has_value())
	{
		const JsonArcingShell &shell = *cfg.abilities.arcingShell;
		if (shell.flightTime.has_value() && *shell.flightTime > 0.0f)
		{
			config.abilities.arcingFlightTime = *shell.flightTime;
			if (shell.splashTiles.has_value() && *shell.splashTiles >= 0)
			{
				config.abilities.arcingSplashTiles = *shell.splashTiles;
			}
		}
	}
	if (cfg.abilities.crushesFlesh.has_value() && *cfg.abilities.crushesFlesh)
	{
		config.abilities.crushesFlesh = true;
	}
	if (cfg.abilities.transport.has_value() &&
		cfg.abilities.transport->capacity.has_value() &&
		*cfg.abilities.transport->capacity > 0)
	{
		config.abilities.transportCapacity = *cfg.abilities.transport->capacity;
	}
	if (cfg.abilities.sniper.has_value() && *cfg.abilities.sniper)
	{
		config.abilities.sniper = true;
	}
	if (cfg.collision.bounds.has_value())
	{
		const int left = cfg.collision.bounds->left;
		const int top = cfg.collision.bounds->top;
		const int right = cfg.collision.bounds->right;
		const int bottom = cfg.collision.bounds->bottom;
		if (left >= 0 && top >= 0 && right > left && bottom > top)
		{
			config.boundLeft = left;
			config.boundTop = top;
			config.footprintWidth = right - left;
			config.footprintHeight = bottom - top;
		}
	}
	if (cfg.collision.origin.has_value())
	{
		config.originX = cfg.collision.origin->x;
		config.originY = cfg.collision.origin->y;
	}
	if (!cfg.art.spritePrefix.empty())
	{
		config.spritePrefix = cfg.art.spritePrefix;
	}
	config.atlasIdlePrefix = cfg.art.atlasIdlePrefix;
	config.atlasMovePrefix = cfg.art.atlasMovePrefix;

	return config;
}
