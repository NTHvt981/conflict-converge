#pragma once

#include <vector>

#include "Registry.h"
#include "Unit.h"

struct Building;

// Damage resolution: typed attacker damage vs defender armor scales
// attackPower into effective damage through a matrix.

// Multiplier for dealt damage against the given armor.
float Effectiveness(DamageType dealt, ArmorType armor);

// Apply scaled damage and restart the attacker's cooldown; returns the
// effective damage dealt. A landed hit stamps the defender's feedback fields.
// Seconds a victim flashes after taking a hit (overlay + number).
inline constexpr float kHitFlashDuration = 0.25f;
float ResolveAttack(Unit &attacker, Unit &defender);

// Structural damage: raw attackPower, no armor matrix. Restarts the cooldown.
float ResolveBuildingAttack(Unit &attacker, Building &building);

// Attack-ground: fires at a world position, hitting the nearest live enemy
// within one tile; empty ground is a clean miss.
void ResolveGroundAttack(Registry &registry, Unit &attacker, Vector2 pos);

// True for vehicle-hull types (IFV/Artillery/Light/HeavyTank).
bool IsVehicleHull(UnitType attackerType);

// 2D hitbox system: every unit body is a 32x32 rect centered in its tile.

// 32x32 body rect for a unit at its current position.
Rectangle HitboxOf(const Unit &unit);
// True when the two bodies overlap (raylib semantics: edge-touch doesn't count).
bool HitboxesOverlap(const Unit &a, const Unit &b);
// Collect live units whose hitbox intersects `area` (teamID < 0 = all teams).
void QueryUnitsInRect(Registry &registry, Rectangle area, int teamID, std::vector<Entity> &out);
