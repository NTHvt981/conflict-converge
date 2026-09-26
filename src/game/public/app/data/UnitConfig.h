#pragma once

#include <string>
#include <vector>

#include "units/UnitStats.h"

// Gimmick abilities (M1 schema, all optional, default off). Tolerant
// per-field load over DefaultUnitConfig; unknown ability keys reject.
struct UnitAbilities
{
    float turretTurnRate = 0.0f;    // >0 = turreted (rad/s traverse)
    float arcingFlightTime = 0.0f;  // >0 = arcing shell (seconds)
    int arcingSplashTiles = 0;      // AoE radius in tiles
    bool crushesFlesh = false;      // heavy-tank crush rule
    int transportCapacity = 0;      // >0 = IFV transport seats (foot only)
    bool sniper = false;            // one-shot-a-sprite tuning flag
};

// Standalone unit-config editor data layer: one UnitConfig per
// data/configs/<type>.json file. Pure logic, no raylib.
struct UnitConfig
{
    std::string type; // UnitType enum name, e.g. "RifleInfantry"
    UnitStats stats;
    int costIron = 0;
    int costOil = 0;
    UnitAbilities abilities;
    // Collision, bounds/origin style: footprint size is derived as
    // (right-left) x (bottom-top); boundLeft/Top is the offset from the
    // anchor tile; originX/Y is the anchor within the footprint.
    int footprintWidth = 1;  // tiles, derived
    int footprintHeight = 1; // tiles, derived
    int boundLeft = 0;       // tiles
    int boundTop = 0;        // tiles
    int originX = 0;         // tiles
    int originY = 0;         // tiles
    std::string spritePrefix;
    std::string atlasIdlePrefix; // "" = flat-PNG fallback
    std::string atlasWalkPrefix; // "" = flat-PNG fallback
};

// Hardcoded baseline for `type`; used when a type has no file and as the
// per-field fallback below.
UnitConfig DefaultUnitConfig(UnitType type);

// Enum <-> enumerator-spelling names ("RUBBER", "KINETIC", "RifleInfantry");
// Parse returns false on unknown names.
bool ParseUnitTypeName(const std::string &name, UnitType &out);
bool ParseArmorTypeName(const std::string &name, ArmorType &out);
bool ParseDamageTypeName(const std::string &name, DamageType &out);
const char *ArmorTypeName(ArmorType type);
const char *DamageTypeName(DamageType type);

// Canonical file stem for `type` in data/configs/ (snake_case for
// multi-word types).
const char *UnitConfigFilename(UnitType type);

// Parse one file's contents; `filenameStem` is the file's own stem.
// Unknown/mismatched type or unparseable JSON returns false.
bool ParseUnitConfigJson(const std::string &json, const std::string &filenameStem,
                         UnitConfig &out);

// Single-file load/save; Save pretty-prints the hand-editable JSON style.
bool LoadUnitConfig(const std::string &path, UnitConfig &out);
bool SaveUnitConfig(const std::string &path, const UnitConfig &config);
std::string UnitConfigToJson(const UnitConfig &config);

// Enumerate dir/*.json, keeping only files that parse; sorted by path.
std::vector<UnitConfig> LoadAllUnitConfigs(const std::string &dir);

// Runtime catalog (M1): gameplay reads stats/costs through the loaded
// configs, not kTable/CostOf-switch directly. Starts as compiled defaults;
// Refresh loads data/configs/*.json with per-type fallback. Headless tests
// without data/ keep running on the defaults.
const UnitConfig &ActiveUnitConfig(UnitType type);
void SetActiveUnitConfigs(const std::vector<UnitConfig> &configs);
void ResetActiveUnitConfigs();
bool RefreshActiveUnitConfigs(const std::string &dir);
// Search the repo-root-relative candidates (data/configs, ../../, ../../../).
bool RefreshActiveUnitConfigsFromSearch();
