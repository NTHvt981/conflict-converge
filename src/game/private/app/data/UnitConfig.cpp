#include "app/data/UnitConfig.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "units/UnitTypeInfo.h"

namespace
{

bool ReadWholeFile(const std::string &path, std::string &out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		return false;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}




}

const char *UnitConfigFilename(UnitType type)
{
	return InfoOf(type).fileStem;
}




bool LoadUnitConfig(const std::string &path, UnitConfig &out)
{
	std::string json;
	if (!ReadWholeFile(path, json))
	{
		return false;
	}
	const std::string stem = std::filesystem::path(path).stem().string();
	return ParseUnitConfigJson(json, stem, out);
}


bool SaveUnitConfig(const std::string &path, const UnitConfig &config)
{
	const std::string json = UnitConfigToJson(config);
	if (json.empty())
	{
		return false;
	}
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
	{
		return false;
	}
	out << json;
	return static_cast<bool>(out);
}
