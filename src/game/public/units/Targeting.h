#pragma once

#include <unordered_map>

#include "Registry.h"
#include "Unit.h"

class FogOfWar;
struct Building;

// Targeting: nearest-enemy acquisition with threat priority; range checks
// gate attacks.

float DistanceBetween(const Unit &a, const Unit &b);

// Priority multiplier on a candidate's threat score: armored seekers and
// AntiArmorInfantry prefer vehicles. 1.0 = neutral.
float TargetPriorityWeight(UnitType seekerType, UnitType candidateType);

// Frame-scoped reserved lethal damage per target (overkill protection).
using ReservedDamageMap = std::unordered_map<Entity, float>;

// Nearest living enemy within seeker.sightRange, highest attackPower first.
// fog gates on visibility (Artillery blind-fires exempt); reserved skips
// already-doomed candidates; nullptr = legacy.
Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr,
                     const ReservedDamageMap *reserved = nullptr);

// Nearest Operational enemy building within sightRange (same fog gate).
Entity AcquireBuildingTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr);

// Footprint center in world pixels (raze/repair aim point).
Vector2 BuildingCenter(const Building &building);

// Circle check: target within attacker.attackRange pixels.
bool InAttackRange(const Unit &attacker, const Unit &target);
