#pragma once

#include <vector> // Hitbox query results

#include "Registry.h" // Entity, Registry::Each for area queries
#include "Unit.h" // DamageType, ArmorType, Unit (no cycle: Unit.h never includes Combat.h)

struct Building; // fwd-decl (Combat.cpp includes Building.h)

// Damage resolution. Attackers deal typed damage (DamageType), defenders
// resist by armor (ArmorType); the matrix scales raw attackPower into
// effective damage. Phases and hit feedback layer on top of ResolveAttack —
// they never bypass it.

// Multiplier for dealt damage against the given armor (placeholder C&C/CoH
// flavor; retune pass adjusts).
float Effectiveness(DamageType dealt, ArmorType armor);

// Apply scaled damage to the defender and restart the attacker's cooldown.
// Returns the effective damage dealt. A landed hit (effective > 0) stamps the
// defender's feedback fields (lastDamageTaken + hitFlashTime); a zero-damage
// hit leaves them untouched.
// Seconds a victim flashes after taking a hit (overlay + number).
inline constexpr float kHitFlashDuration = 0.25f;
float ResolveAttack(Unit &attacker, Unit &defender);

// Structural damage. Raw attackPower, no armor matrix (structures are
// untyped): wrecks production so games terminate. Restarts the cooldown and
// returns effective damage like ResolveAttack. The driver demolishes at zero.
float ResolveBuildingAttack(Unit &attacker, Building &building);

// QoL attack-ground: fires `attacker`'s attack at a world position.
// Single-target (no splash falloff — that's a separate balance feature):
// the nearest live enemy within one tile (64px) of `pos` takes a Direct
// ResolveAttack; empty ground is a clean miss (the attacker still cycled).
void ResolveGroundAttack(Registry &registry, Unit &attacker, Vector2 pos);

// Vehicle-hull classification (IFV/Artillery/Light/HeavyTank). Used by
// Targeting's priority matrix (armor hunters prefer vehicle targets).
bool IsVehicleHull(UnitType attackerType);

// 2D hitbox system. Every unit body is a 32x32 rect centered in its 64x64
// tile (matches the placeholder art in main.cpp; real sprites' blocking
// rects stay within the same box). Range checks stay circle-based;
// hitboxes serve contact/area queries (attack-ground's nearest-impact
// search, splash later) via raylib's rect collision.

// 32x32 body rect for a unit at its current position.
Rectangle HitboxOf(const Unit &unit);
// True when the two bodies overlap (raylib rect semantics: touching edges
// alone do not count).
bool HitboxesOverlap(const Unit &a, const Unit &b);
// Collect live units whose hitbox intersects `area`. teamID < 0 matches all
// teams; otherwise only that team. Dead/missing entries are never included.
void QueryUnitsInRect(Registry &registry, Rectangle area, int teamID, std::vector<Entity> &out);
