#pragma once

#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "units/Deployment.h"
#include "world/TileMap.h"

inline constexpr float kDeployExitDelayPerMember = 0.22f;
inline constexpr float kDeployWalkSpeedMultiplier = 1.2f;
inline constexpr float kDeploySlotParkRadius = 3.0f;

void BeginDeploy(Registry &registry, Entity unitId, Entity producerId,
				 const TileMap &map, OccupancyGrid *occ, int controlGroupBit);

void UpdateDeploy(Registry &registry, const TileMap &map, OccupancyGrid *occ, float dt);

void AssembleDeploy(Registry &registry, const TileMap &map, OccupancyGrid *occ,
					Entity unitId);
