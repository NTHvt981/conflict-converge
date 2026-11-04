#include "app/data/AIConfig.h"

#define CEREAL_RAPIDJSON_ASSERT(x) \
	do { if (!(x)) throw std::runtime_error("malformed ai-config JSON"); } while (0)

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

template <class Archive, class T>
void TryLoadOptional(Archive &ar, const char *name, std::optional<T> &out)
{
	T value{};
	if (TryLoadKey(ar, name, value))
	{
		out = value;
	}
	else
	{
		out = std::nullopt;
	}
}

struct JsonAIDifficulty
{
	std::optional<int> harvesters;
	std::optional<int> waveThreshold;
	std::optional<int> reserveUnits;
	std::optional<float> scoutInterval;
	std::optional<float> relaunchCooldown;
	std::optional<bool> retreats;
	std::vector<std::string> composition;
	template <class Archive>
	void load(Archive &ar)
	{
		TryLoadOptional(ar, "harvesters", harvesters);
		TryLoadOptional(ar, "waveThreshold", waveThreshold);
		TryLoadOptional(ar, "reserveUnits", reserveUnits);
		TryLoadOptional(ar, "scoutInterval", scoutInterval);
		TryLoadOptional(ar, "relaunchCooldown", relaunchCooldown);
		TryLoadOptional(ar, "retreats", retreats);
		TryLoadKey(ar, "composition", composition);
	}
};

const char *DifficultyKey(AIDifficulty difficulty)
{
	switch (difficulty)
	{
	case AIDifficulty::Easy:
		return "easy";
	case AIDifficulty::Medium:
		return "medium";
	case AIDifficulty::Hard:
		return "hard";
	}
	return "medium";
}

std::array<AIDifficultyParams, 3> gActiveAI;
bool gAIInit = false;

void EnsureAIDefaults()
{
	if (gAIInit)
	{
		return;
	}
	gActiveAI = { ParamsFor(AIDifficulty::Easy), ParamsFor(AIDifficulty::Medium),
				  ParamsFor(AIDifficulty::Hard) };
	gAIInit = true;
}

int DifficultyIndex(AIDifficulty difficulty)
{
	return static_cast<int>(difficulty);
}

} // namespace

std::expected<AIDifficultyParams, ConfigError>
ParseAIDifficultyJson(const std::string &json, AIDifficulty difficulty)
{
	AIDifficultyParams params = ParamsFor(difficulty);
	try
	{
		std::stringstream ss(json);
		cereal::JSONInputArchive ar(ss);
		JsonAIDifficulty block;
		ar(cereal::make_nvp(DifficultyKey(difficulty), block));
		if (block.harvesters.has_value())
		{
			params.harvesters = *block.harvesters;
		}
		if (block.waveThreshold.has_value())
		{
			params.waveThreshold = *block.waveThreshold;
		}
		if (block.reserveUnits.has_value())
		{
			params.reserveUnits = *block.reserveUnits;
		}
		if (block.scoutInterval.has_value())
		{
			params.scoutInterval = *block.scoutInterval;
		}
		if (block.relaunchCooldown.has_value())
		{
			params.relaunchCooldown = *block.relaunchCooldown;
		}
		if (block.retreats.has_value())
		{
			params.retreats = *block.retreats;
		}
		if (!block.composition.empty())
		{
			std::vector<UnitType> composition;
			for (const std::string &name : block.composition)
			{
				const std::expected<UnitType, ConfigError> type = ParseUnitTypeName(name);
				if (!type)
				{
					return std::unexpected(type.error());
				}
				composition.push_back(*type);
			}
			params.composition = composition;
		}
	}
	catch (const cereal::Exception &)
	{
		return std::unexpected(ConfigError::MalformedJson);
	}
	catch (const std::runtime_error &)
	{
		return std::unexpected(ConfigError::MalformedJson);
	}
	return params;
}

std::expected<AIDifficultyParams, ConfigError>
LoadAIDifficultyFile(const std::string &path, AIDifficulty difficulty)
{
	std::ifstream in(path);
	if (!in)
	{
		return std::unexpected(ConfigError::FileUnreadable);
	}
	std::stringstream ss;
	ss << in.rdbuf();
	return ParseAIDifficultyJson(ss.str(), difficulty);
}

const AIDifficultyParams &ActiveAIDifficultyParams(AIDifficulty difficulty)
{
	EnsureAIDefaults();
	return gActiveAI[static_cast<std::size_t>(DifficultyIndex(difficulty))];
}

void SetActiveAIDifficultyParams(AIDifficulty difficulty, const AIDifficultyParams &params)
{
	EnsureAIDefaults();
	gActiveAI[static_cast<std::size_t>(DifficultyIndex(difficulty))] = params;
}

void ResetAIDifficultyParams()
{
	gAIInit = false;
	EnsureAIDefaults();
}

bool RefreshAIDifficultyParams(const std::string &dir)
{
	const std::string path = (std::filesystem::path(dir) / "ai.json").string();
	std::ifstream probe(path);
	if (!probe)
	{
		return false;
	}
	std::array<AIDifficultyParams, 3> loaded = { ParamsFor(AIDifficulty::Easy),
												 ParamsFor(AIDifficulty::Medium),
												 ParamsFor(AIDifficulty::Hard) };
	const AIDifficulty difficulties[] = { AIDifficulty::Easy, AIDifficulty::Medium,
										  AIDifficulty::Hard };
	for (AIDifficulty difficulty : difficulties)
	{
		const std::expected<AIDifficultyParams, ConfigError> load =
			LoadAIDifficultyFile(path, difficulty);
		if (!load)
		{
			TraceLog(LOG_WARNING, "ai config %s rejected: %s", path.c_str(),
					 Describe(load.error()));
			return false;
		}
		loaded[static_cast<std::size_t>(difficulty)] = *load;
	}
	gActiveAI = loaded;
	gAIInit = true;
	return true;
}

bool RefreshAIDifficultyParamsFromSearch()
{
	static const char *kDirs[] = { "data/configs", "../../data/configs",
								   "../../../data/configs" };
	for (const char *dir : kDirs)
	{
		if (RefreshAIDifficultyParams(dir))
		{
			return true;
		}
	}
	return false;
}
