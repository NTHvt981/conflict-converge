#pragma once

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
