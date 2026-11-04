#pragma once

#include <expected>
#include <string>

#include "app/data/ConfigError.h"
#include "units/AICommander.h"

[[nodiscard]] std::expected<AIDifficultyParams, ConfigError>
ParseAIDifficultyJson(const std::string &json, AIDifficulty difficulty);
[[nodiscard]] std::expected<AIDifficultyParams, ConfigError>
LoadAIDifficultyFile(const std::string &path, AIDifficulty difficulty);

const AIDifficultyParams &ActiveAIDifficultyParams(AIDifficulty difficulty);
void SetActiveAIDifficultyParams(AIDifficulty difficulty, const AIDifficultyParams &params);
void ResetAIDifficultyParams();
bool RefreshAIDifficultyParams(const std::string &dir);
bool RefreshAIDifficultyParamsFromSearch();
