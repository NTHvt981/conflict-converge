#pragma once

#include "Registry.h" // Registry, Entity, kInvalidEntity
#include "Unit.h"     // Unit

class FogOfWar; // fwd-decl (Targeting.cpp includes FogOfWar.h)

// M3 Goal 4: targeting logic. Acquisition scans for the nearest enemy with
// threat priority; range checks gate attacks (M3G5 state machine, M4 damage).

float DistanceBetween(const Unit &a, const Unit &b);

// Nearest living enemy of a different team within seeker.sightRange.
// Threat priority: highest attackPower wins, ties broken by distance.
// Returns kInvalidEntity when the seeker is missing or nothing qualifies.
// M9: with fog, candidates on tiles unseen by the seeker's team are skipped —
// except for Artillery, which blind-fires into shroud at no penalty (Q78).
Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr);

// Circle check: target within attacker.attackRange pixels.
bool InAttackRange(const Unit &attacker, const Unit &target);
