#include "app/data/CombatRules.h"

#define CEREAL_RAPIDJSON_ASSERT(x) \
	do { if (!(x)) throw std::runtime_error("malformed combat-rules JSON"); } while (0)

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "raylib.h"

#include "app/data/UnitConfig.h"

namespace
{

template <class Archive, class T>
bool TryLoadKey(Archive &ar, const char *name, T &out)
{
	try
	{
		ar(cereal::make_nvp(name, out));
		return true;
	}
	catch (const cereal::Exception &)
	{
		return false;
	}
}

struct UnknownConfigName
{
};

bool ValidCell(float cell)
{
	return std::isfinite(cell) && cell > 0.0f;
}

struct MatrixJson
{
	float values[3][3] = {};
	bool seen[3][3] = {};

	template <class Archive>
	void load(Archive &ar)
	{
		while (const char *damageName = ar.getNodeName())
		{
			const std::expected<DamageType, ConfigError> damage =
				ParseDamageTypeName(damageName);
			if (!damage)
			{
				throw UnknownConfigName{};
			}
			ar.startNode();
			while (const char *armorName = ar.getNodeName())
			{
				const std::expected<ArmorType, ConfigError> armor =
					ParseArmorTypeName(armorName);
				if (!armor)
				{
					throw UnknownConfigName{};
				}
				float cell = 0.0f;
				ar(cell);
				const int row = static_cast<int>(*damage);
				const int col = static_cast<int>(*armor);
				values[row][col] = cell;
				seen[row][col] = true;
			}
			ar.finishNode();
		}
	}

	std::expected<CombatRules, ConfigError> result() const
	{
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				if (!seen[row][col] || !ValidCell(values[row][col]))
				{
					return std::unexpected(ConfigError::InvalidData);
				}
			}
		}
		CombatRules rules{};
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				rules.damageMatrix[row][col] = values[row][col];
			}
		}
		return rules;
	}
};

} // namespace

std::expected<CombatRules, ConfigError>
ParseCombatRulesJson(const std::string &json)
{
	MatrixJson parsed;
	std::expected<CombatRules, ConfigError> result = std::unexpected(ConfigError::MalformedJson);
	try
	{
		std::stringstream ss(json);
		cereal::JSONInputArchive ar(ss);
		if (TryLoadKey(ar, "damageMatrix", parsed))
		{
			result = parsed.result();
		}
		else
		{
			result = std::unexpected(ConfigError::InvalidData);
		}
	}
	catch (const UnknownConfigName &)
	{
		return std::unexpected(ConfigError::UnknownName);
	}
	catch (const std::runtime_error &)
	{
		return std::unexpected(ConfigError::MalformedJson);
	}
	return result;
}

std::expected<CombatRules, ConfigError>
LoadCombatRulesFile(const std::string &path)
{
	std::ifstream in(path);
	if (!in)
	{
		return std::unexpected(ConfigError::FileUnreadable);
	}
	std::stringstream ss;
	ss << in.rdbuf();
	return ParseCombatRulesJson(ss.str());
}

namespace
{

CombatRules gActiveRules = {};
bool gRulesInit = false;

void EnsureRulesDefaults()
{
	if (gRulesInit)
	{
		return;
	}
	gActiveRules = DefaultCombatRules();
	gRulesInit = true;
}

} // namespace

CombatRules DefaultCombatRules()
{
	static constexpr float kDefaults[3][3] = {
		{ 1.00f, 0.75f, 0.50f },
		{ 1.25f, 1.00f, 0.75f },
		{ 0.50f, 1.25f, 1.00f },
	};
	CombatRules rules{};
	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 3; ++col)
		{
			rules.damageMatrix[row][col] = kDefaults[row][col];
		}
	}
	return rules;
}

const CombatRules &ActiveCombatRules()
{
	EnsureRulesDefaults();
	return gActiveRules;
}

void SetActiveCombatRules(const CombatRules &rules)
{
	EnsureRulesDefaults();
	gActiveRules = rules;
}

void ResetCombatRules()
{
	gRulesInit = false;
	EnsureRulesDefaults();
}

bool RefreshCombatRules(const std::string &dir)
{
	const std::string path = (std::filesystem::path(dir) / "combat_rules.json").string();
	std::ifstream probe(path);
	if (!probe)
	{
		return false;
	}
	const std::expected<CombatRules, ConfigError> loaded = LoadCombatRulesFile(path);
	if (!loaded)
	{
		TraceLog(LOG_WARNING, "combat rules %s rejected: %s", path.c_str(),
				 Describe(loaded.error()));
		return false;
	}
	SetActiveCombatRules(*loaded);
	return true;
}

bool RefreshCombatRulesFromSearch()
{
	static const char *kDirs[] = { "data/configs", "../../data/configs",
								   "../../../data/configs" };
	for (const char *dir : kDirs)
	{
		if (RefreshCombatRules(dir))
		{
			return true;
		}
	}
	return false;
}
