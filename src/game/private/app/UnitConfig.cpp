#include "UnitConfig.h"

#define CEREAL_RAPIDJSON_ASSERT(x) \
    do { if (!(x)) throw std::runtime_error("malformed unit-config JSON"); } while (0)

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

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

const char *BaselineSpritePrefix(UnitType type)
{
    switch (type)
    {
    case UnitType::RifleInfantry:
        return "rifle_infantry";
    case UnitType::AntiArmorInfantry:
        return "antiarmor";
    case UnitType::Engineer:
        return "engineer";
    case UnitType::IFV:
        return "ifv";
    case UnitType::Artillery:
        return "artillery";
    case UnitType::LightTank:
        return "lighttank";
    case UnitType::HeavyTank:
        return "heavytank";
    case UnitType::Count:
        break;
    }
    return "rifle_infantry";
}

bool BaselineIsVehicle(UnitType type)
{
    return type == UnitType::IFV || type == UnitType::Artillery ||
           type == UnitType::LightTank || type == UnitType::HeavyTank;
}

const char *UnitTypeConfigName(UnitType type)
{
    switch (type)
    {
    case UnitType::RifleInfantry:
        return "RifleInfantry";
    case UnitType::AntiArmorInfantry:
        return "AntiArmorInfantry";
    case UnitType::Engineer:
        return "Engineer";
    case UnitType::IFV:
        return "IFV";
    case UnitType::Artillery:
        return "Artillery";
    case UnitType::LightTank:
        return "LightTank";
    case UnitType::HeavyTank:
        return "HeavyTank";
    case UnitType::PrototypeInfantry:
        return "PrototypeInfantry";
    case UnitType::Count:
        break;
    }
    return "";
}

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
void TryLoadValue(Archive &ar, const char *camelName, const char *snakeName, T &out)
{
    if (!TryLoadKey(ar, camelName, out) && snakeName != nullptr)
    {
        TryLoadKey(ar, snakeName, out);
    }
}

template <class Archive, class T>
void TryLoadValue(Archive &ar, const char *name, T &out)
{
    TryLoadValue(ar, name, nullptr, out);
}

template <class Archive, class T>
void TryLoadOptional(Archive &ar, const char *camelName, const char *snakeName,
                     std::optional<T> &out)
{
    T value{};
    if (TryLoadKey(ar, camelName, value) ||
        (snakeName != nullptr && TryLoadKey(ar, snakeName, value)))
    {
        out = value;
    }
    else
    {
        out = std::nullopt;
    }
}

struct JsonStats
{
    std::optional<float> health;
    std::string armorType;
    std::string damageType;
    std::optional<int> attackPower;
    std::optional<int> attackRange;
    std::optional<float> cooldownTime;
    std::optional<float> speed;
    std::optional<float> sightRange;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "health", nullptr, health);
        TryLoadValue(ar, "armorType", "armor_type", armorType);
        TryLoadValue(ar, "damageType", "damage_type", damageType);
        TryLoadOptional(ar, "attackPower", "attack_power", attackPower);
        TryLoadOptional(ar, "attackRange", "attack_range", attackRange);
        TryLoadOptional(ar, "cooldownTime", "cooldown_time", cooldownTime);
        TryLoadOptional(ar, "speed", nullptr, speed);
        TryLoadOptional(ar, "sightRange", "sight_range", sightRange);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("health", health.value()),
           cereal::make_nvp("armorType", armorType),
           cereal::make_nvp("damageType", damageType),
           cereal::make_nvp("attackPower", attackPower.value()),
           cereal::make_nvp("attackRange", attackRange.value()),
           cereal::make_nvp("cooldownTime", cooldownTime.value()),
           cereal::make_nvp("speed", speed.value()),
           cereal::make_nvp("sightRange", sightRange.value()));
    }
};

struct JsonBounds
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "left", left);
        TryLoadValue(ar, "top", top);
        TryLoadValue(ar, "right", right);
        TryLoadValue(ar, "bottom", bottom);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(CEREAL_NVP(left), CEREAL_NVP(top), CEREAL_NVP(right), CEREAL_NVP(bottom));
    }
};

struct JsonOrigin
{
    int x = 0;
    int y = 0;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "x", x);
        TryLoadValue(ar, "y", y);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(CEREAL_NVP(x), CEREAL_NVP(y));
    }
};

struct JsonCollision
{
    std::optional<JsonBounds> bounds;
    std::optional<JsonOrigin> origin;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "bounds", nullptr, bounds);
        TryLoadOptional(ar, "origin", nullptr, origin);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("bounds", bounds.value()),
           cereal::make_nvp("origin", origin.value()));
    }
};

struct JsonArt
{
    std::string spritePrefix;
    std::string atlasIdlePrefix;
    std::string atlasWalkPrefix;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadValue(ar, "spritePrefix", "sprite_prefix", spritePrefix);
        TryLoadValue(ar, "atlasIdlePrefix", "atlas_idle_prefix", atlasIdlePrefix);
        TryLoadValue(ar, "atlasWalkPrefix", "atlas_walk_prefix", atlasWalkPrefix);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(CEREAL_NVP(spritePrefix), CEREAL_NVP(atlasIdlePrefix), CEREAL_NVP(atlasWalkPrefix));
    }
};

struct JsonConfig
{
    std::string type;
    JsonStats stats;
    JsonCollision collision;
    JsonArt art;
};

}

const char *UnitConfigFilename(UnitType type)
{
    switch (type)
    {
    case UnitType::RifleInfantry:
        return "rifle_infantry";
    case UnitType::AntiArmorInfantry:
        return "antiarmor_infantry";
    case UnitType::Engineer:
        return "engineer";
    case UnitType::IFV:
        return "ifv";
    case UnitType::Artillery:
        return "artillery";
    case UnitType::LightTank:
        return "light_tank";
    case UnitType::HeavyTank:
        return "heavy_tank";
    case UnitType::PrototypeInfantry:
        return "prototype_infantry";
    case UnitType::Count:
        break;
    }
    return "";
}

UnitConfig DefaultUnitConfig(UnitType type)
{
    UnitConfig config;
    config.type = UnitTypeConfigName(type);
    config.stats = BaseStats(type);
    config.footprintWidth = BaselineIsVehicle(type) ? 2 : 1;
    config.footprintHeight = BaselineIsVehicle(type) ? 2 : 1;
    config.spritePrefix = BaselineSpritePrefix(type);
    if (type == UnitType::RifleInfantry)
    {
        config.atlasIdlePrefix = "rifle_infantry_idle";
        config.atlasWalkPrefix = "rifle_infantry_walk";
    }
    return config;
}

bool ParseUnitTypeName(const std::string &name, UnitType &out)
{
    if (name == "RifleInfantry" || name == "Infantry") // "Infantry" is the pre-rename alias
    {
        out = UnitType::RifleInfantry;
        return true;
    }
    if (name == "AntiArmorInfantry")
    {
        out = UnitType::AntiArmorInfantry;
        return true;
    }
    if (name == "Engineer")
    {
        out = UnitType::Engineer;
        return true;
    }
    if (name == "IFV")
    {
        out = UnitType::IFV;
        return true;
    }
    if (name == "Artillery")
    {
        out = UnitType::Artillery;
        return true;
    }
    if (name == "LightTank")
    {
        out = UnitType::LightTank;
        return true;
    }
    if (name == "HeavyTank")
    {
        out = UnitType::HeavyTank;
        return true;
    }
    if (name == "PrototypeInfantry")
    {
        out = UnitType::PrototypeInfantry;
        return true;
    }
    return false;
}

bool ParseArmorTypeName(const std::string &name, ArmorType &out)
{
    if (name == "STEEL")
    {
        out = ArmorType::STEEL;
        return true;
    }
    if (name == "RUBBER")
    {
        out = ArmorType::RUBBER;
        return true;
    }
    if (name == "COMPOSITE")
    {
        out = ArmorType::COMPOSITE;
        return true;
    }
    return false;
}

bool ParseDamageTypeName(const std::string &name, DamageType &out)
{
    if (name == "KINETIC")
    {
        out = DamageType::KINETIC;
        return true;
    }
    if (name == "EXPLOSIVE")
    {
        out = DamageType::EXPLOSIVE;
        return true;
    }
    if (name == "ENERGY")
    {
        out = DamageType::ENERGY;
        return true;
    }
    return false;
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

bool ParseUnitConfigJson(const std::string &json, const std::string &filenameStem,
                         UnitConfig &out)
{
    JsonConfig cfg;
    try
    {
        std::istringstream in(json);
        cereal::JSONInputArchive ar(in);
        TryLoadValue(ar, "type", cfg.type);
        TryLoadValue(ar, "stats", cfg.stats);
        TryLoadValue(ar, "collision", cfg.collision);
        TryLoadValue(ar, "art", cfg.art);
    }
    catch (const std::exception &)
    {
        return false;
    }
    UnitType type = UnitType::RifleInfantry;
    if (!ParseUnitTypeName(cfg.type, type))
    {
        return false;
    }
    if (NormalizedStem(cfg.type) != NormalizedStem(filenameStem))
    {
        return false;
    }

    UnitConfig config = DefaultUnitConfig(type);
    const JsonStats &stats = cfg.stats;
    if (stats.health.has_value() && *stats.health > 0.0f)
    {
        config.stats.health = *stats.health;
    }
    ArmorType armor = config.stats.armorType;
    if (!stats.armorType.empty() && ParseArmorTypeName(stats.armorType, armor))
    {
        config.stats.armorType = armor;
    }
    DamageType damage = config.stats.damageType;
    if (!stats.damageType.empty() && ParseDamageTypeName(stats.damageType, damage))
    {
        config.stats.damageType = damage;
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
    config.atlasWalkPrefix = cfg.art.atlasWalkPrefix;

    out = config;
    return true;
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

std::string UnitConfigToJson(const UnitConfig &config)
{
    JsonConfig out;
    out.type = config.type;
    out.stats.health = config.stats.health;
    out.stats.armorType = ArmorTypeName(config.stats.armorType);
    out.stats.damageType = DamageTypeName(config.stats.damageType);
    out.stats.attackPower = config.stats.attackPower;
    out.stats.attackRange = config.stats.attackRange;
    out.stats.cooldownTime = config.stats.cooldownTime;
    out.stats.speed = config.stats.speed;
    out.stats.sightRange = config.stats.sightRange;
    JsonBounds bounds;
    bounds.left = config.boundLeft;
    bounds.top = config.boundTop;
    bounds.right = config.boundLeft + config.footprintWidth;
    bounds.bottom = config.boundTop + config.footprintHeight;
    out.collision.bounds = bounds;
    JsonOrigin origin;
    origin.x = config.originX;
    origin.y = config.originY;
    out.collision.origin = origin;
    out.art.spritePrefix = config.spritePrefix;
    out.art.atlasIdlePrefix = config.atlasIdlePrefix;
    out.art.atlasWalkPrefix = config.atlasWalkPrefix;

    std::ostringstream json;
    {
        cereal::JSONOutputArchive ar(json);
        ar(cereal::make_nvp("type", out.type), cereal::make_nvp("stats", out.stats),
           cereal::make_nvp("collision", out.collision), cereal::make_nvp("art", out.art));
    }
    return json.str();
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
