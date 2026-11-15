#pragma once

#include <unordered_map>

#include "core/Registry.h"
#include "units/Unit.h"

class FogOfWar;
struct Building;

float DistanceBetween(const Unit &a, const Unit &b);

// 1.0 = neutral.
float TargetPriorityWeight(UnitType seekerType, UnitType candidateType);

// Build cost as worth: pricier targets out-pull cheap ones at equal power.
float ThreatWorth(UnitType type);

enum class ThreatScope
{
	Units,
	Buildings
};

// Frame-scoped reserved lethal damage per target (overkill protection).
using ReservedDamageMap = std::unordered_map<Entity, float>;

Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr,
					 const ReservedDamageMap *reserved = nullptr);

// Nearest Operational enemy building in sightRange, fog-gated.
Entity AcquireBuildingTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr);

// Footprint center in world pixels.
Vector2 BuildingCenter(const Building &building);

bool InAttackRange(const Unit &attacker, const Unit &target);
