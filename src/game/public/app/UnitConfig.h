#pragma once

#include <string>
#include <vector>

#include "UnitStats.h"

// Standalone unit-config editor data layer: one UnitConfig per
// data/configs/<type>.json file. Pure logic, no raylib.
struct UnitConfig
{
    std::string type; // UnitType enum name, e.g. "Infantry"
    UnitStats stats;
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

// Enum <-> enumerator-spelling names ("RUBBER", "KINETIC", "Infantry");
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
