#pragma once

#include <vector>

#include "core/Registry.h"
#include "core/Team.h"
#include "units/Unit.h"

struct Building;
class TileMap;

float Effectiveness(DamageType dealt, ArmorType armor);

// Seconds a victim flashes after a hit.
inline constexpr float kHitFlashDuration = 0.25f;
float ResolveAttack(Unit &attacker, CombatState &attackerCombat, Unit &defender,
					CombatState &defenderCombat);

float ResolveBuildingAttack(Unit &attacker, CombatState &attackerCombat, Building &building);

void ResolveGroundAttack(Registry &registry, Unit &attacker, CombatState &attackerCombat,
						 Vector2 pos);

// attackPower <= 0 still means "cannot attack"; shells are never persisted.
void ResolveStrike(Registry &registry, Entity attackerId, Unit &attacker,
					CombatState &attackerCombat, Entity targetId);
void ResolveStrikeGround(Registry &registry, Entity attackerId, Unit &attacker,
						 CombatState &attackerCombat, Vector2 pos);

void UpdateProjectiles(Registry &registry, TileMap &map, float dtSeconds);

bool IsVehicleHull(UnitType attackerType);

// Half-tile rect centered in the unit's tile.
Rectangle HitboxOf(const Unit &unit);
// Edge-touch doesn't count (raylib semantics).
bool HitboxesOverlap(const Unit &a, const Unit &b);
void QueryUnitsInRect(Registry &registry, Rectangle area, TeamId teamID, std::vector<Entity> &out);
