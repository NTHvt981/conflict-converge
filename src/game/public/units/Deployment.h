#pragma once

#include <vector>

#include "core/MathUtils.h"
#include "core/Registry.h"

struct Deployment
{
	Entity producer = kInvalidEntity;
	int membersTotal = 0;
	int membersSpawned = 0;
	int membersDied = 0;
	float nextSpawnAt = 0.0f;
	float exitDelay = 0.0f;
	cc::IVec2 doorTile{ -1, -1 };
	std::vector<cc::Vec2> memberSpawnedPos;
};

inline bool IsDeploying(const Registry &registry, Entity id)
{
	return registry.Has<Deployment>(id);
}
