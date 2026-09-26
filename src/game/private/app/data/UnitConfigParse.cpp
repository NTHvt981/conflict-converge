
#include "app/data/UnitConfig.h"

#define CEREAL_RAPIDJSON_ASSERT(x) \
    do { if (!(x)) throw std::runtime_error("malformed unit-config JSON"); } while (0)

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "units/UnitStats.h"

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
    case UnitType::Medic:
        return "medic";
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

void BaselineCost(UnitType type, int &iron, int &oil)
{
    switch (type)
    {
    case UnitType::RifleInfantry:
        iron = 25;
        oil = 0;
        break;
    case UnitType::AntiArmorInfantry:
        iron = 30;
        oil = 5;
        break;
    case UnitType::Engineer:
        iron = 40;
        oil = 0;
        break;
    case UnitType::IFV:
        iron = 80;
        oil = 20;
        break;
    case UnitType::Artillery:
        iron = 120;
        oil = 40;
        break;
    case UnitType::LightTank:
        iron = 150;
        oil = 50;
        break;
    case UnitType::HeavyTank:
        iron = 250;
        oil = 100;
        break;
    case UnitType::PrototypeInfantry:
        iron = 25;
        oil = 0;
        break;
    case UnitType::Medic:
        iron = 30;
        oil = 0;
        break;
    case UnitType::Count:
        iron = 0;
        oil = 0;
        break;
    }
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
    case UnitType::Medic:
        return "Medic";
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

struct JsonCost
{
    std::optional<int> iron;
    std::optional<int> oil;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "iron", nullptr, iron);
        TryLoadOptional(ar, "oil", nullptr, oil);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("iron", iron.value()), cereal::make_nvp("oil", oil.value()));
    }
};

struct JsonTurret
{
    std::optional<float> turnRate;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "turnRate", "turn_rate", turnRate);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("turnRate", turnRate.value()));
    }
};

struct JsonArcingShell
{
    std::optional<float> flightTime;
    std::optional<int> splashTiles;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "flightTime", "flight_time", flightTime);
        TryLoadOptional(ar, "splashTiles", "splash_tiles", splashTiles);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("flightTime", flightTime.value()),
           cereal::make_nvp("splashTiles", splashTiles.value()));
    }
};

struct JsonTransport
{
    std::optional<int> capacity;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "capacity", nullptr, capacity);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("capacity", capacity.value()));
    }
};

struct JsonAbilities
{
    std::optional<JsonTurret> turret;
    std::optional<JsonArcingShell> arcingShell;
    std::optional<bool> crushesFlesh;
    std::optional<JsonTransport> transport;
    std::optional<bool> sniper;
    template <class Archive>
    void load(Archive &ar)
    {
        TryLoadOptional(ar, "turret", nullptr, turret);
        TryLoadOptional(ar, "arcingShell", "arcing_shell", arcingShell);
        TryLoadOptional(ar, "crushesFlesh", "crushes_flesh", crushesFlesh);
        TryLoadOptional(ar, "transport", nullptr, transport);
        TryLoadOptional(ar, "sniper", nullptr, sniper);
    }
    template <class Archive>
    void save(Archive &ar) const
    {
        ar(cereal::make_nvp("turret", turret.value()),
           cereal::make_nvp("arcingShell", arcingShell.value()),
           cereal::make_nvp("crushesFlesh", crushesFlesh.value()),
           cereal::make_nvp("transport", transport.value()),
           cereal::make_nvp("sniper", sniper.value()));
    }
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

UnitConfig DefaultUnitConfig(UnitType type)
{
    UnitConfig config;
    config.type = UnitTypeConfigName(type);
    config.stats = BaseStats(type);
    BaselineCost(type, config.costIron, config.costOil);
    config.abilities = UnitAbilities{};
    if (type == UnitType::HeavyTank)
    {
        config.abilities.crushesFlesh = true;
    }
    // G1: turreted tanks aim body-independent heads (visible traverse).
    if (type == UnitType::LightTank || type == UnitType::HeavyTank)
    {
        config.abilities.turretTurnRate = 3.0f;
    }
    // G2: artillery lobs dodgeable shells (visible arc, small AoE). Tuned
    // so Medium closes soak games: 0.6s flight, splash 1 (foot cannot
    // outwalk ±32px + hitbox, vehicles still dodge).
    if (type == UnitType::Artillery)
    {
        config.abilities.arcingFlightTime = 0.6f;
        config.abilities.arcingSplashTiles = 1;
    }
    // G4: IFV carries foot infantry (player gestures only; the AI never
    // builds carriers or boards — transport AI is a separate project).
    if (type == UnitType::IFV)
    {
        config.abilities.transportCapacity = 4;
    }
    // G5: AntiArmorInfantry is the designated marksman — 25 power is ~25%
    // of a 100HP squad, so every hit visibly removes one soldier through
    // the existing SquadSlots thresholds. True per-sprite HP deferred.
    if (type == UnitType::AntiArmorInfantry)
    {
        config.abilities.sniper = true;
    }
    config.footprintWidth = BaselineIsVehicle(type) ? 2 : 1;
    config.footprintHeight = BaselineIsVehicle(type) ? 2 : 1;
    config.spritePrefix = BaselineSpritePrefix(type);
    if (type == UnitType::RifleInfantry)
    {
        config.atlasIdlePrefix = "rifle_infantry_idle";
        config.atlasWalkPrefix = "rifle_infantry_walk";
    }
    if (type == UnitType::AntiArmorInfantry)
    {
        config.atlasIdlePrefix = "antiarmor_idle";
        config.atlasWalkPrefix = "antiarmor_walk";
    }
    if (type == UnitType::Engineer)
    {
        config.atlasIdlePrefix = "engineer_idle";
        config.atlasWalkPrefix = "engineer_walk";
    }
    if (type == UnitType::Medic)
    {
        config.atlasIdlePrefix = "medic_idle";
        config.atlasWalkPrefix = "medic_walk";
    }
    return config;
}

bool ParseUnitTypeName(const std::string &name, UnitType &out)
{
    if (name == "RifleInfantry" || name == "Infantry") // "Infantry" alias
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
    if (name == "Medic")
    {
        out = UnitType::Medic;
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
    if (HasUnknownAbilityKey(json))
    {
        return false;
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
    config.atlasWalkPrefix = cfg.art.atlasWalkPrefix;

    out = config;
    return true;
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
    out.cost.iron = config.costIron;
    out.cost.oil = config.costOil;
    {
        JsonTurret turret;
        turret.turnRate = config.abilities.turretTurnRate;
        out.abilities.turret = turret;
        JsonArcingShell shell;
        shell.flightTime = config.abilities.arcingFlightTime;
        shell.splashTiles = config.abilities.arcingSplashTiles;
        out.abilities.arcingShell = shell;
        out.abilities.crushesFlesh = config.abilities.crushesFlesh;
        JsonTransport transport;
        transport.capacity = config.abilities.transportCapacity;
        out.abilities.transport = transport;
        out.abilities.sniper = config.abilities.sniper;
    }
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
           cereal::make_nvp("cost", out.cost), cereal::make_nvp("abilities", out.abilities),
           cereal::make_nvp("collision", out.collision), cereal::make_nvp("art", out.art));
    }
    return json.str();
}
