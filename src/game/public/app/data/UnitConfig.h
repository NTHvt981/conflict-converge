#pragma once

#include <expected>
#include <string>
#include <vector>

#include "app/data/ConfigError.h"
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
	std::string atlasMovePrefix; // "" = flat-PNG fallback
};

// Hardcoded baseline for `type`; used when a type has no file and as the
// per-field fallback below.
UnitConfig DefaultUnitConfig(UnitType type);

// Enum <-> enumerator-spelling names ("RUBBER", "KINETIC", "RifleInfantry").
[[nodiscard]] std::expected<UnitType, ConfigError> ParseUnitTypeName(const std::string &name);
[[nodiscard]] std::expected<ArmorType, ConfigError> ParseArmorTypeName(const std::string &name);
[[nodiscard]] std::expected<DamageType, ConfigError> ParseDamageTypeName(const std::string &name);
const char *ArmorTypeName(ArmorType type);
const char *DamageTypeName(DamageType type);

// Canonical file stem for `type` in data/configs/ (snake_case for
// multi-word types).
const char *UnitConfigFilename(UnitType type);

// True when `stem` is a unit type's canonical file stem. data/configs/ also
// holds the atlas and AI documents, so the catalog only reports rejections
// for names that claim to be unit configs.
bool IsUnitConfigStem(const std::string &stem);

// Parse one file's contents; `filenameStem` is the file's own stem. A
// mismatched or unknown type rejects the whole file.
[[nodiscard]] std::expected<UnitConfig, ConfigError>
ParseUnitConfigJson(const std::string &json, const std::string &filenameStem);

// Single-file load/save; Save pretty-prints the hand-editable JSON style.
[[nodiscard]] std::expected<UnitConfig, ConfigError> LoadUnitConfig(const std::string &path);
[[nodiscard]] std::expected<void, ConfigError> SaveUnitConfig(const std::string &path,
                                                             const UnitConfig &config);
std::string UnitConfigToJson(const UnitConfig &config);

// Enumerate dir/*.json, keeping only files that parse; sorted by path. Each
// rejected file logs one warning naming its cause.
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
