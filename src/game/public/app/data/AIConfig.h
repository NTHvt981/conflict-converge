#pragma once

#include <string>

#include "units/AICommander.h"

bool ParseAIDifficultyJson(const std::string &json, AIDifficulty difficulty,
						   AIDifficultyParams &out);
bool LoadAIDifficultyFile(const std::string &path, AIDifficulty difficulty,
						  AIDifficultyParams &out);

const AIDifficultyParams &ActiveAIDifficultyParams(AIDifficulty difficulty);
void SetActiveAIDifficultyParams(AIDifficulty difficulty, const AIDifficultyParams &params);
void ResetAIDifficultyParams();
bool RefreshAIDifficultyParams(const std::string &dir);
bool RefreshAIDifficultyParamsFromSearch();
