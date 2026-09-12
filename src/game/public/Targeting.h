#pragma once

#include "Registry.h" // Registry, Entity, kInvalidEntity
#include "Unit.h"     // Unit

class FogOfWar; // fwd-decl (Targeting.cpp includes FogOfWar.h)
struct Building; // fwd-decl (Targeting.cpp includes Building.h)

// M3 Goal 4: targeting logic. Acquisition scans for the nearest enemy with
// threat priority; range checks gate attacks (M3G5 state machine, M4 damage).

float DistanceBetween(const Unit &a, const Unit &b);

// Nearest living enemy of a different team within seeker.sightRange.
// Threat priority: highest attackPower wins, ties broken by distance.
// Returns kInvalidEntity when the seeker is missing or nothing qualifies.
// M9: with fog, candidates on tiles unseen by the seeker's team are skipped —
// except for Artillery, which blind-fires into shroud at no penalty (Q78).
Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr);

// M13: nearest Operational enemy building within sightRange. Same fog gate
// (artillery exempt); wrecked, own-team, and unseen structures are skipped.
// Lets marches raze production so games terminate (snowball via demolition).
Entity AcquireBuildingTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr);

// M13: footprint center in world pixels (raze aim point, repair aim point).
Vector2 BuildingCenter(const Building &building);

// Circle check: target within attacker.attackRange pixels.
bool InAttackRange(const Unit &attacker, const Unit &target);
