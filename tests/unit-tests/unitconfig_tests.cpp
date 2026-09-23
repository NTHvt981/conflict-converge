// Unit tests for the standalone unit-config editor data layer (schema
// parse/serialize round-trip, directory discovery, tolerant-load cases,
// enum round-trips). Draw-call code (preview pane, overlay) stays untested
// per project convention — verified visually when the editor runs.

#include "test_harness.h"

#include "UnitConfig.h"
#include "UnitStats.h"

#include <filesystem>
#include <fstream>

namespace
{

bool ConfigsEqual(const UnitConfig &a, const UnitConfig &b)
{
    return a.type == b.type && a.stats.health == b.stats.health &&
           a.stats.armorType == b.stats.armorType &&
           a.stats.damageType == b.stats.damageType &&
           a.stats.attackPower == b.stats.attackPower &&
           a.stats.attackRange == b.stats.attackRange &&
           a.stats.cooldownTime == b.stats.cooldownTime &&
           a.stats.speed == b.stats.speed && a.stats.sightRange == b.stats.sightRange &&
           a.footprintWidth == b.footprintWidth &&
           a.footprintHeight == b.footprintHeight && a.boundLeft == b.boundLeft &&
           a.boundTop == b.boundTop && a.originX == b.originX && a.originY == b.originY &&
           a.spritePrefix == b.spritePrefix && a.atlasIdlePrefix == b.atlasIdlePrefix &&
           a.atlasWalkPrefix == b.atlasWalkPrefix;
}

std::string TempDir(const char *name)
{
    return (std::filesystem::temp_directory_path() / name).string();
}

void WriteFile(const std::string &path, const char *contents)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
}

// data/configs at repo root; the test binary may run from prj/bin/Debug.
const char *kConfigDirs[] = { "data/configs", "../../data/configs",
                              "../../../data/configs" };

} // namespace

void RunUnitConfigTests()
{
    // --- single-file round-trip: save, reload, field-for-field equal ---
    {
        UnitConfig heavy = DefaultUnitConfig(UnitType::HeavyTank);
        const std::string json = UnitConfigToJson(heavy);
        CC_CHECK(!json.empty());
        UnitConfig reloaded;
        CC_CHECK(ParseUnitConfigJson(json, "heavy_tank", reloaded));
        CC_CHECK(ConfigsEqual(heavy, reloaded));
    }

    // --- round-trip preserves edited (non-default) values ---
    {
        UnitConfig edited = DefaultUnitConfig(UnitType::RifleInfantry);
        edited.stats.health = 150.0f;
        edited.stats.armorType = ArmorType::COMPOSITE;
        edited.stats.attackPower = 0; // explicit zero survives (not a default)
        edited.boundLeft = 1;       // bounds {1,0,3,1}: footprint derives 2x1
        edited.footprintWidth = 2;
        edited.originX = 1; // stored + round-trips, no gameplay effect yet
        edited.spritePrefix = "custom";
        edited.atlasIdlePrefix = "custom_idle";
        const std::string json = UnitConfigToJson(edited);
        UnitConfig reloaded;
        CC_CHECK(ParseUnitConfigJson(json, "rifle_infantry", reloaded));
        CC_CHECK(ConfigsEqual(edited, reloaded));
    }

    // --- canonical filenames: snake_case stems, underscore-tolerant match ---
    {
        CC_CHECK(std::string(UnitConfigFilename(UnitType::RifleInfantry)) == "rifle_infantry");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::AntiArmorInfantry)) ==
                 "antiarmor_infantry");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::Engineer)) == "engineer");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::IFV)) == "ifv");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::Artillery)) == "artillery");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::LightTank)) == "light_tank");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::HeavyTank)) == "heavy_tank");
        CC_CHECK(std::string(UnitConfigFilename(UnitType::PrototypeInfantry)) ==
                 "prototype_infantry");

        // Underscored stems validate; a genuinely different type still fails.
        UnitConfig config;
        const std::string heavy = UnitConfigToJson(DefaultUnitConfig(UnitType::HeavyTank));
        CC_CHECK(ParseUnitConfigJson(heavy, "heavy_tank", config));
        CC_CHECK(ParseUnitConfigJson(heavy, "HEAVY_TANK", config)); // case-insensitive
        CC_CHECK(!ParseUnitConfigJson(heavy, "light_tank", config));
    }

    // --- shipped seed files load with the compiled-in values ---
    {
        const std::vector<UnitConfig> *found = nullptr;
        std::vector<UnitConfig> configs;
        for (const char *dir : kConfigDirs)
        {
            configs = LoadAllUnitConfigs(dir);
            if (!configs.empty())
            {
                found = &configs;
                break;
            }
        }
        CC_CHECK(found != nullptr);
        if (found != nullptr)
        {
            CC_CHECK(found->size() == 9);
            bool rifleOk = false;
            bool antiarmorOk = false;
            bool engineerOk = false;
            bool medicOk = false;
            bool tankOk = false;
            for (const UnitConfig &config : *found)
            {
                if (config.type == "RifleInfantry")
                {
                    const UnitStats &base = BaseStats(UnitType::RifleInfantry);
                    CC_CHECK(config.stats.health == base.health);
                    CC_CHECK(config.stats.attackPower == base.attackPower);
                    CC_CHECK(config.footprintWidth == 1);
                    CC_CHECK(config.spritePrefix == "rifle_infantry");
                    CC_CHECK(config.atlasIdlePrefix == "rifle_infantry_idle");
                    rifleOk = true;
                }
                if (config.type == "AntiArmorInfantry")
                {
                    CC_CHECK(config.spritePrefix == "antiarmor");
                    CC_CHECK(config.atlasIdlePrefix == "antiarmor_idle");
                    CC_CHECK(config.atlasWalkPrefix == "antiarmor_walk");
                    antiarmorOk = true;
                }
                if (config.type == "Engineer")
                {
                    CC_CHECK(config.spritePrefix == "engineer");
                    CC_CHECK(config.atlasIdlePrefix == "engineer_idle");
                    CC_CHECK(config.atlasWalkPrefix == "engineer_walk");
                    engineerOk = true;
                }
                if (config.type == "Medic")
                {
                    CC_CHECK(config.stats.attackPower == 0); // support: cannot attack
                    CC_CHECK(config.spritePrefix == "medic");
                    CC_CHECK(config.atlasIdlePrefix == "medic_idle");
                    CC_CHECK(config.atlasWalkPrefix == "medic_walk");
                    medicOk = true;
                }
                if (config.type == "HeavyTank")
                {
                    CC_CHECK(config.stats.armorType == ArmorType::COMPOSITE);
                    CC_CHECK(config.footprintWidth == 2);
                    CC_CHECK(config.atlasIdlePrefix.empty()); // PNG fallback
                    tankOk = true;
                }
            }
            CC_CHECK(rifleOk);
            CC_CHECK(antiarmorOk);
            CC_CHECK(engineerOk);
            CC_CHECK(medicOk);
            CC_CHECK(tankOk);
        }
    }

    // --- directory discovery: 3 valid + 1 garbage -> exactly 3 ---
    {
        const std::string dir = TempDir("cc_unitconfig_discovery");
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        WriteFile(dir + "/rifle_infantry.json",
                  UnitConfigToJson(DefaultUnitConfig(UnitType::RifleInfantry)).c_str());
        WriteFile(dir + "/engineer.json",
                  UnitConfigToJson(DefaultUnitConfig(UnitType::Engineer)).c_str());
        WriteFile(dir + "/ifv.json",
                  UnitConfigToJson(DefaultUnitConfig(UnitType::IFV)).c_str());
        WriteFile(dir + "/garbage.json", "{ this is not json");
        WriteFile(dir + "/notes.txt", "wrong extension, ignored");

        const std::vector<UnitConfig> configs = LoadAllUnitConfigs(dir);
        CC_CHECK(configs.size() == 3);

        std::filesystem::remove_all(dir, ec);
    }

    // --- missing dir -> empty, not a crash ---
    {
        CC_CHECK(LoadAllUnitConfigs(TempDir("cc_unitconfig_no_such_dir")).empty());
    }

    // --- tolerant load: missing file -> false (caller falls back) ---
    {
        UnitConfig config;
        CC_CHECK(!LoadUnitConfig(TempDir("cc_unitconfig_missing.json"), config));
    }

    // --- tolerant load: type/filename mismatch + unknown type skipped ---
    {
        UnitConfig config;
        const std::string rifle =
            UnitConfigToJson(DefaultUnitConfig(UnitType::RifleInfantry));
        CC_CHECK(!ParseUnitConfigJson(rifle, "engineer", config)); // mismatch
        CC_CHECK(!ParseUnitConfigJson("{\"type\":\"Spaceship\"}", "spaceship",
                                      config)); // unknown
        CC_CHECK(!ParseUnitConfigJson("{ nope", "rifle_infantry", config)); // garbage
        CC_CHECK(!ParseUnitConfigJson("{\"stats\":{}}", "rifle_infantry", config)); // no type
    }

    // --- tolerant load: missing/unknown/out-of-range fields -> defaults ---
    {
        UnitConfig config;
        // Missing everything but type: full baseline.
        CC_CHECK(ParseUnitConfigJson("{\"type\":\"Artillery\"}", "artillery", config));
        const UnitConfig baseline = DefaultUnitConfig(UnitType::Artillery);
        CC_CHECK(ConfigsEqual(baseline, config));

        // Unknown enum names + bad bounds fall back per-field; the
        // well-formed remainder still loads.
        CC_CHECK(ParseUnitConfigJson(
            "{\"type\":\"LightTank\",\"stats\":{\"health\":-5,\"armorType\":\"WOOD\","
            "\"damageType\":\"\",\"attackPower\":21},\"collision\":{\"bounds\":{"
            "\"left\":0,\"top\":0,\"right\":0,\"bottom\":3},\"origin\":{\"x\":1,\"y\":0}},"
            "\"art\":{\"spritePrefix\":\"\",\"atlasIdlePrefix\":\"x\"}}",
            "light_tank", config));
        const UnitConfig tankBase = DefaultUnitConfig(UnitType::LightTank);
        CC_CHECK(config.stats.health == tankBase.stats.health); // -5 rejected
        CC_CHECK(config.stats.armorType == tankBase.stats.armorType); // WOOD rejected
        CC_CHECK(config.stats.damageType == tankBase.stats.damageType); // "" rejected
        CC_CHECK(config.stats.attackPower == 21); // explicit honored
        CC_CHECK(config.footprintWidth == tankBase.footprintWidth); // degenerate rejected
        CC_CHECK(config.footprintHeight == tankBase.footprintHeight);
        CC_CHECK(config.boundLeft == 0 && config.boundTop == 0);
        CC_CHECK(config.originX == 1 && config.originY == 0); // origin always stored
        CC_CHECK(config.spritePrefix == tankBase.spritePrefix); // "" rejected
        CC_CHECK(config.atlasIdlePrefix == "x"); // "" is valid: explicit honored

        // Well-formed bounds derive the footprint (offset rect, not just size).
        CC_CHECK(ParseUnitConfigJson(
            "{\"type\":\"LightTank\",\"collision\":{\"bounds\":{"
            "\"left\":1,\"top\":0,\"right\":3,\"bottom\":2},\"origin\":{\"x\":0,\"y\":0}}}",
            "light_tank", config));
        CC_CHECK(config.footprintWidth == 2 && config.footprintHeight == 2);
        CC_CHECK(config.boundLeft == 1 && config.boundTop == 0);
    }

    // --- enum round-trips: every value serializes and parses back ---
    {
        for (int i = 0; i < static_cast<int>(ArmorType::Count); ++i)
        {
            const ArmorType armor = static_cast<ArmorType>(i);
            ArmorType back = ArmorType::STEEL;
            CC_CHECK(ParseArmorTypeName(ArmorTypeName(armor), back));
            CC_CHECK(back == armor);
        }
        for (int i = 0; i < static_cast<int>(DamageType::Count); ++i)
        {
            const DamageType damage = static_cast<DamageType>(i);
            DamageType back = DamageType::KINETIC;
            CC_CHECK(ParseDamageTypeName(DamageTypeName(damage), back));
            CC_CHECK(back == damage);
        }
        for (int i = 0; i < static_cast<int>(UnitType::Count); ++i)
        {
            const UnitType type = static_cast<UnitType>(i);
            const UnitConfig baseline = DefaultUnitConfig(type);
            UnitType back = UnitType::RifleInfantry;
            CC_CHECK(ParseUnitTypeName(baseline.type, back));
            CC_CHECK(back == type);
        }
        ArmorType noArmor = ArmorType::STEEL;
        DamageType noDamage = DamageType::KINETIC;
        UnitType noUnit = UnitType::RifleInfantry;
        CC_CHECK(!ParseArmorTypeName("WOOD", noArmor));
        CC_CHECK(!ParseDamageTypeName("NUCLEAR", noDamage));
        CC_CHECK(!ParseUnitTypeName("Spaceship", noUnit));
        // Pre-rename "Infantry" stays a valid alias for old configs/saves.
        CC_CHECK(ParseUnitTypeName("Infantry", noUnit));
        CC_CHECK(noUnit == UnitType::RifleInfantry);
    }
}
