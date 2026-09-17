#include "UnitConfig.h"

#include <algorithm> // LoadAllUnitConfigs sorted-by-path order
#include <cctype> // tolerant filename/type comparison
#include <filesystem>
#include <fstream>
#include <sstream>

#include <google/protobuf/util/json_util.h>

#include "unitconfig.pb.h"

namespace
{

std::string Lower(std::string s)
{
    for (char &c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
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

// Canonical-until-section-5 duplicates (each points at its live source):
// the game still hardcodes these, so the no-file baseline mirrors them.
const char *BaselineSpritePrefix(UnitType type)
{
    switch (type)
    {
    case UnitType::Infantry:
        return "infantry";
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
    return "infantry"; // unreachable; mirrors Art.cpp's UnitFile fallback
}

bool BaselineIsVehicle(UnitType type)
{
    // Mirrors ApplyBaseStats' inline split (UnitStats.cpp) until the
    // section-5 follow-up replaces both with a config lookup.
    return type == UnitType::IFV || type == UnitType::Artillery ||
           type == UnitType::LightTank || type == UnitType::HeavyTank;
}

// Exact enumerator spellings for the config "type" field ("AntiArmorInfantry",
// not Hud.h UnitTypeName's display text "Anti-Armor" — the schema requires
// the enum name so filenames and the type field validate against each other).
const char *UnitTypeConfigName(UnitType type)
{
    switch (type)
    {
    case UnitType::Infantry:
        return "Infantry";
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
    case UnitType::Count:
        break;
    }
    return "";
}

} // namespace

UnitConfig DefaultUnitConfig(UnitType type)
{
    UnitConfig config;
    config.type = UnitTypeConfigName(type);
    config.stats = BaseStats(type);
    config.footprintWidth = BaselineIsVehicle(type) ? 2 : 1;
    config.footprintHeight = BaselineIsVehicle(type) ? 2 : 1;
    config.spritePrefix = BaselineSpritePrefix(type);
    // Only Infantry resolves atlas names in the game today
    // (data/sprites.json has no other entries); the rest fall back to
    // flat PNGs, represented here as empty prefixes.
    if (type == UnitType::Infantry)
    {
        config.atlasIdlePrefix = "infantry_idle";
        config.atlasWalkPrefix = "infantry_walk";
    }
    return config;
}

bool ParseUnitTypeName(const std::string &name, UnitType &out)
{
    if (name == "Infantry")
    {
        out = UnitType::Infantry;
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
    cc::unitconfig::UnitConfig proto;
    if (!google::protobuf::util::JsonStringToMessage(json, &proto).ok())
    {
        return false;
    }
    UnitType type = UnitType::Infantry;
    if (!ParseUnitTypeName(proto.type(), type))
    {
        return false; // missing or unrecognized UnitType
    }
    if (Lower(proto.type()) != Lower(filenameStem))
    {
        return false; // type doesn't match its own filename
    }

    UnitConfig config = DefaultUnitConfig(type);
    const cc::unitconfig::UnitStatsConfig &stats = proto.stats();
    if (stats.has_health() && stats.health() > 0.0f)
    {
        config.stats.health = stats.health();
    }
    ArmorType armor = config.stats.armorType;
    if (!stats.armor_type().empty() && ParseArmorTypeName(stats.armor_type(), armor))
    {
        config.stats.armorType = armor;
    }
    DamageType damage = config.stats.damageType;
    if (!stats.damage_type().empty() && ParseDamageTypeName(stats.damage_type(), damage))
    {
        config.stats.damageType = damage;
    }
    if (stats.has_attack_power())
    {
        config.stats.attackPower = stats.attack_power();
    }
    if (stats.has_attack_range())
    {
        config.stats.attackRange = stats.attack_range();
    }
    if (stats.has_cooldown_time())
    {
        config.stats.cooldownTime = stats.cooldown_time();
    }
    if (stats.has_speed())
    {
        config.stats.speed = stats.speed();
    }
    if (stats.has_sight_range())
    {
        config.stats.sightRange = stats.sight_range();
    }
    if (proto.collision().footprint_width() >= 1)
    {
        config.footprintWidth = proto.collision().footprint_width();
    }
    if (proto.collision().footprint_height() >= 1)
    {
        config.footprintHeight = proto.collision().footprint_height();
    }
    if (!proto.art().sprite_prefix().empty())
    {
        config.spritePrefix = proto.art().sprite_prefix();
    }
    // Atlas prefixes: empty is the valid "flat-PNG fallback" state.
    config.atlasIdlePrefix = proto.art().atlas_idle_prefix();
    config.atlasWalkPrefix = proto.art().atlas_walk_prefix();

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
    cc::unitconfig::UnitConfig proto;
    proto.set_type(config.type);
    cc::unitconfig::UnitStatsConfig *stats = proto.mutable_stats();
    stats->set_health(config.stats.health);
    stats->set_armor_type(ArmorTypeName(config.stats.armorType));
    stats->set_damage_type(DamageTypeName(config.stats.damageType));
    stats->set_attack_power(config.stats.attackPower);
    stats->set_attack_range(config.stats.attackRange);
    stats->set_cooldown_time(config.stats.cooldownTime);
    stats->set_speed(config.stats.speed);
    stats->set_sight_range(config.stats.sightRange);
    proto.mutable_collision()->set_footprint_width(config.footprintWidth);
    proto.mutable_collision()->set_footprint_height(config.footprintHeight);
    proto.mutable_art()->set_sprite_prefix(config.spritePrefix);
    proto.mutable_art()->set_atlas_idle_prefix(config.atlasIdlePrefix);
    proto.mutable_art()->set_atlas_walk_prefix(config.atlasWalkPrefix);

    std::string json;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true; // hand-editable style, like sprites.json
    if (!google::protobuf::util::MessageToJsonString(proto, &json, options).ok())
    {
        return {};
    }
    return json;
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
        return configs; // missing/unreadable dir: editor falls back to defaults
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
            continue; // unparseable files never reach the editor list
        }
        configs.push_back(std::move(config));
    }
    std::sort(configs.begin(), configs.end(),
              [](const UnitConfig &a, const UnitConfig &b) { return a.type < b.type; });
    return configs;
}
