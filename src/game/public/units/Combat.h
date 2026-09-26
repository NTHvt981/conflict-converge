#pragma once

#include <vector>

#include "core/Registry.h"
#include "units/Unit.h"

struct Building;
class TileMap;

float Effectiveness(DamageType dealt, ArmorType armor);

// Seconds a victim flashes after taking a hit (overlay + number).
inline constexpr float kHitFlashDuration = 0.25f;
float ResolveAttack(Unit &attacker, CombatState &attackerCombat, Unit &defender,
                    CombatState &defenderCombat);

float ResolveBuildingAttack(Unit &attacker, CombatState &attackerCombat, Building &building);

// Attack-ground: fires at a world position, hitting the nearest live enemy
// within one tile; empty ground is a clean miss.
void ResolveGroundAttack(Registry &registry, Unit &attacker, CombatState &attackerCombat,
                         Vector2 pos);

// attackPower <= 0 still means "cannot attack". In-flight shells are NOT persisted.
void ResolveStrike(Registry &registry, Entity attackerId, Unit &attacker,
                    CombatState &attackerCombat, Entity targetId);
void ResolveStrikeGround(Registry &registry, Entity attackerId, Unit &attacker,
                         CombatState &attackerCombat, Vector2 pos);

// Call once per frame from the movement pipeline.
void UpdateProjectiles(Registry &registry, TileMap &map, float dtSeconds);

bool IsVehicleHull(UnitType attackerType);

// Every unit body is a 32x32 rect centered in its tile.
Rectangle HitboxOf(const Unit &unit);
// Edge-touch doesn't count (raylib semantics).
bool HitboxesOverlap(const Unit &a, const Unit &b);
// teamID < 0 = all teams.
void QueryUnitsInRect(Registry &registry, Rectangle area, int teamID, std::vector<Entity> &out);
