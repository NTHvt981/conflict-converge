#pragma once

#include <string>
#include <vector>

#include "UnitStats.h" // UnitStats baseline + ArmorType/DamageType/UnitType

// Standalone unit-config editor data layer (see
// plans/Standalone_Unit_Config_Editor_Plan.md section 1): one UnitConfig
// per data/configs/<type>.json file. Pure logic, no raylib — the editor
// UI, the game-side follow-up, and the unit tests all share this.
struct UnitConfig
{
    std::string type; // UnitType enum name, e.g. "Infantry"
    UnitStats stats;
    // Collision, bounds/origin style (see the schema doc in
    // proto/unitconfig.proto): gameplay footprint size is derived as
    // (right-left) x (bottom-top); boundLeft/Top is the footprint offset
    // from the anchor tile, originX/Y the anchor within the footprint
    // (today always top-left-anchored: offsets 0, origin 0).
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

// Hardcoded baseline for `type` (today's compiled-in behavior: kTable
// stats, vehicle 2x2 / else 1x1, UnitFile prefix, Infantry atlas pair).
// Used when a type has no file yet and as the per-field fallback below.
UnitConfig DefaultUnitConfig(UnitType type);

// Enum <-> enumerator-spelling names ("RUBBER", "KINETIC", "Infantry").
// Parse returns false on unknown names. (UnitType -> name is a local
// exact-spelling switch, deliberately NOT Hud.h's UnitTypeName — that one
// returns display text like "Anti-Armor", which would fail this file's
// own type/filename validation on save+reload.)
bool ParseUnitTypeName(const std::string &name, UnitType &out);
bool ParseArmorTypeName(const std::string &name, ArmorType &out);
bool ParseDamageTypeName(const std::string &name, DamageType &out);
const char *ArmorTypeName(ArmorType type);
const char *DamageTypeName(DamageType type);

// Canonical file stem for `type` in data/configs/ ("antiarmor_infantry",
// not a mechanical lowercasing of "AntiArmorInfantry" — multi-word types
// use snake_case). The editor saves through this; the loader accepts it.
const char *UnitConfigFilename(UnitType type);

// Parse one file's contents; `filenameStem` is the file's own stem
// ("heavy_tank" — underscores ignored when matching, so the stem just
// needs to spell the type). Unknown/mismatched type, or JSON that doesn't
// parse, returns false (caller skips the file with a warning). Missing fields fall back to DefaultUnitConfig(type);
// out-of-range numerics (health <= 0, footprint < 1) and unknown
// armor/damage names fall back per-field the same way.
bool ParseUnitConfigJson(const std::string &json, const std::string &filenameStem,
                         UnitConfig &out);

// Single-file load/save. Save writes only this type's file,
// pretty-printed to match the hand-editable data/configs JSON style.
bool LoadUnitConfig(const std::string &path, UnitConfig &out);
bool SaveUnitConfig(const std::string &path, const UnitConfig &config);
std::string UnitConfigToJson(const UnitConfig &config);

// Directory discovery (ListMaps pattern): enumerate dir/*.json, keep only
// the files that parse; missing dir -> empty. Sorted by path.
std::vector<UnitConfig> LoadAllUnitConfigs(const std::string &dir);
