#pragma once

#include <expected>
#include <string>

#include "app/data/ConfigError.h"

struct CombatRules
{
	float damageMatrix[3][3]; // [DamageType][ArmorType]
};

CombatRules DefaultCombatRules();
[[nodiscard]] std::expected<CombatRules, ConfigError> ParseCombatRulesJson(const std::string &json);
[[nodiscard]] std::expected<CombatRules, ConfigError> LoadCombatRulesFile(const std::string &path);

const CombatRules &ActiveCombatRules();
void SetActiveCombatRules(const CombatRules &rules);
void ResetCombatRules();
bool RefreshCombatRules(const std::string &dir);
bool RefreshCombatRulesFromSearch();
