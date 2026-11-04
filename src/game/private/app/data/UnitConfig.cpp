#include "app/data/UnitConfig.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "units/UnitTypeInfo.h"

const char *UnitConfigFilename(UnitType type)
{
	return InfoOf(type).fileStem;
}

std::expected<UnitConfig, ConfigError> LoadUnitConfig(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		return std::unexpected(ConfigError::FileUnreadable);
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	const std::string stem = std::filesystem::path(path).stem().string();
	return ParseUnitConfigJson(ss.str(), stem);
}

std::expected<void, ConfigError> SaveUnitConfig(const std::string &path,
                                               const UnitConfig &config)
{
	const std::string json = UnitConfigToJson(config);
	if (json.empty())
	{
		return std::unexpected(ConfigError::InvalidData);
	}
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
	{
		return std::unexpected(ConfigError::FileUnwritable);
	}
	out << json;
	if (!out)
	{
		return std::unexpected(ConfigError::FileUnwritable);
	}
	return {};
}
