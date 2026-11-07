#pragma once

#include <expected>
#include <string>

#include "app/data/ConfigError.h"
#include "app/data/UnitConfig.h"
#include "units/Unit.h"

// True when `stem` is a unit type's canonical file stem. data/configs/ also
// holds the atlas and AI documents, so the catalog only reports rejections
// for names that claim to be unit configs.
bool IsUnitConfigStem(const std::string &stem);

// Enum <-> enumerator-spelling names ("RUBBER", "KINETIC", "RifleInfantry").
[[nodiscard]] std::expected<UnitType, ConfigError> ParseUnitTypeName(const std::string &name);
[[nodiscard]] std::expected<ArmorType, ConfigError> ParseArmorTypeName(const std::string &name);
[[nodiscard]] std::expected<DamageType, ConfigError> ParseDamageTypeName(const std::string &name);
const char *ArmorTypeName(ArmorType type);
const char *DamageTypeName(DamageType type);

// Parse one file's contents; `filenameStem` is the file's own stem. A
// mismatched or unknown type rejects the whole file.
std::string NormalizedStem(std::string s);
bool HasUnknownAbilityKey(const std::string &json);

std::expected<UnitConfig, ConfigError> ParseUnitConfigJson(const std::string &json,
														  const std::string &filenameStem);
