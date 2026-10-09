#pragma once

#include <unordered_map>

#include "core/Registry.h"
#include "units/Unit.h"

class FogOfWar;
struct Building;

float DistanceBetween(const Unit &a, const Unit &b);

// Priority multiplier on a candidate's threat score: armored seekers and
// AntiArmorInfantry prefer vehicles. 1.0 = neutral.
float TargetPriorityWeight(UnitType seekerType, UnitType candidateType);

// Build cost (iron + oil) as target worth: expensive units out-pull cheap
// ones when power is close.
float ThreatWorth(UnitType type);

// Wave destination scope: sighted field forces vs production worth razing.
enum class ThreatScope
{
    Units,
    Buildings
};

// Frame-scoped reserved lethal damage per target (overkill protection).
using ReservedDamageMap = std::unordered_map<Entity, float>;

// Fog gates on visibility (Artillery blind-fires exempt); nullptr reserved =
// no overkill protection.
Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr,
                     const ReservedDamageMap *reserved = nullptr);

// Nearest Operational enemy building within sightRange (same fog gate).
Entity AcquireBuildingTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr);

// Footprint center in world pixels (raze/repair aim point).
Vector2 BuildingCenter(const Building &building);

bool InAttackRange(const Unit &attacker, const Unit &target);
