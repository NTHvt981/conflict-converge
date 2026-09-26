
#include "app/data/UnitConfig.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>


namespace
{

std::vector<UnitConfig> gActiveConfigs;

void EnsureActiveDefaults()
{
    if (!gActiveConfigs.empty())
    {
        return;
    }
    for (int i = 0; i < static_cast<int>(UnitType::Count); ++i)
    {
        gActiveConfigs.push_back(DefaultUnitConfig(static_cast<UnitType>(i)));
    }
}

} // namespace

std::vector<UnitConfig> LoadAllUnitConfigs(const std::string &dir)
{
    std::vector<UnitConfig> configs;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        return configs;
    }
    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec))
    {
        if (ec || !it->is_regular_file(ec) || it->path().extension() != ".json")
        {
            continue;
        }
        UnitConfig config;
        if (!LoadUnitConfig(it->path().string(), config))
        {
            continue;
        }
        configs.push_back(std::move(config));
    }
    std::sort(configs.begin(), configs.end(),
              [](const UnitConfig &a, const UnitConfig &b) { return a.type < b.type; });
    return configs;
}

const UnitConfig &ActiveUnitConfig(UnitType type)
{
    EnsureActiveDefaults();
    const int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(gActiveConfigs.size()))
    {
        EnsureActiveDefaults();
        return gActiveConfigs.front();
    }
    // gActiveConfigs is stored in UnitType order (see Refresh/Set).
    for (const UnitConfig &config : gActiveConfigs)
    {
        UnitType parsed = UnitType::RifleInfantry;
        if (ParseUnitTypeName(config.type, parsed) && parsed == type)
        {
            return config;
        }
    }
    return gActiveConfigs[static_cast<std::size_t>(index)];
}

void SetActiveUnitConfigs(const std::vector<UnitConfig> &configs)
{
    gActiveConfigs.clear();
    for (int i = 0; i < static_cast<int>(UnitType::Count); ++i)
    {
        const UnitType type = static_cast<UnitType>(i);
        bool found = false;
        for (const UnitConfig &config : configs)
        {
            UnitType parsed = UnitType::RifleInfantry;
            if (ParseUnitTypeName(config.type, parsed) && parsed == type)
            {
                gActiveConfigs.push_back(config);
                found = true;
                break;
            }
        }
        if (!found)
        {
            gActiveConfigs.push_back(DefaultUnitConfig(type));
        }
    }
}

void ResetActiveUnitConfigs()
{
    gActiveConfigs.clear();
    EnsureActiveDefaults();
}

bool RefreshActiveUnitConfigs(const std::string &dir)
{
    const std::vector<UnitConfig> loaded = LoadAllUnitConfigs(dir);
    if (loaded.empty())
    {
        return false;
    }
    SetActiveUnitConfigs(loaded);
    return true;
}

bool RefreshActiveUnitConfigsFromSearch()
{
    static const char *kDirs[] = { "data/configs", "../../data/configs",
                                   "../../../data/configs" };
    for (const char *dir : kDirs)
    {
        if (RefreshActiveUnitConfigs(dir))
        {
            return true;
        }
    }
    return false;
}

