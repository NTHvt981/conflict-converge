#pragma once

#include <vector> // M4G2: hitbox query results

#include "Registry.h" // Entity, Registry::Each for area queries
#include "Unit.h" // DamageType, ArmorType, Unit (no cycle: Unit.h never includes Combat.h)

// M4 Goal 1: damage resolution. Attackers deal typed damage (DamageType),
// defenders resist by armor (ArmorType); the matrix scales raw attackPower
// into effective damage. Anti-crush (M4G4), phases (M4G3), and hit feedback
// (M4G5) layer on top of ResolveAttack — they never bypass it.

// Multiplier for dealt damage against the given armor (placeholder C&C/CoH
// flavor; M4 retune pass adjusts after playtesting).
float Effectiveness(DamageType dealt, ArmorType armor);

// Apply scaled damage to the defender and restart the attacker's cooldown.
// Returns the effective damage dealt.
float ResolveAttack(Unit &attacker, Unit &defender);

// M4 Goal 2: 2D hitbox system. Every unit body is a 32x32 rect centered in
// its 64x64 tile (matches the placeholder art in main.cpp; real sprites in
// M4 blockings stay within the same box). Range checks (M3G4) stay
// circle-based; hitboxes serve contact/area queries (crush in M4G4, splash
// later) via raylib's rect collision.

// 32x32 body rect for a unit at its current position.
Rectangle HitboxOf(const Unit &unit);
// True when the two bodies overlap (raylib rect semantics: touching edges
// alone do not count).
bool HitboxesOverlap(const Unit &a, const Unit &b);
// Collect live units whose hitbox intersects `area`. teamID < 0 matches all
// teams; otherwise only that team. Dead/missing entries are never included.
void QueryUnitsInRect(Registry &registry, Rectangle area, int teamID, std::vector<Entity> &out);
