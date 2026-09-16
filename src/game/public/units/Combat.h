#pragma once

#include <vector> // M4G2: hitbox query results

#include "Registry.h" // Entity, Registry::Each for area queries
#include "Unit.h" // DamageType, ArmorType, Unit (no cycle: Unit.h never includes Combat.h)

struct Building; // fwd-decl (Combat.cpp includes Building.h)

// M4 Goal 1: damage resolution. Attackers deal typed damage (DamageType),
// defenders resist by armor (ArmorType); the matrix scales raw attackPower
// into effective damage. Anti-crush (M4G4), phases (M4G3), and hit feedback
// (M4G5) layer on top of ResolveAttack — they never bypass it.

// Multiplier for dealt damage against the given armor (placeholder C&C/CoH
// flavor; M4 retune pass adjusts after playtesting).
float Effectiveness(DamageType dealt, ArmorType armor);

// Apply scaled damage to the defender and restart the attacker's cooldown.
// Returns the effective damage dealt. Crush hits (contact/overrun, as
// opposed to Direct fire) are negated against crush-protected foot units.
// A landed hit (effective > 0) stamps the defender's M4G5 feedback fields
// (lastDamageTaken + hitFlashTime); negated/zero hits leave them untouched.
enum class AttackContext
{
    Direct, // ranged/melee fire: full matrix damage
    Crush   // vehicle contact: negated vs foot (M4G4), matrix otherwise
};

// Seconds a victim flashes after taking a hit (M4G5 overlay + number).
inline constexpr float kHitFlashDuration = 0.25f;
float ResolveAttack(Unit &attacker, Unit &defender, AttackContext context = AttackContext::Direct);

// M13: structural damage. Raw attackPower, no armor matrix (structures are
// untyped): wrecks production so games terminate. Restarts the cooldown and
// returns effective damage like ResolveAttack. The driver demolishes at zero.
float ResolveBuildingAttack(Unit &attacker, Building &building);

// QoL attack-ground: fires `attacker`'s attack at a world position.
// Single-target (no splash falloff — that's a separate balance feature):
// the nearest live enemy within one tile (64px) of `pos` takes a Direct
// ResolveAttack; empty ground is a clean miss (the attacker still cycled).
void ResolveGroundAttack(Registry &registry, Unit &attacker, Vector2 pos);

// M4 Goal 4: anti-crush rule. Vehicle hulls (IFV/Artillery/Light/HeavyTank)
// cannot crush foot units (Infantry/AntiArmor/Engineer) — the overrun deals
// no damage. Vehicle-vs-vehicle rams and foot-vs-anything crushes still use
// the matrix.
bool IsVehicleHull(UnitType attackerType);
bool IsCrushNegated(UnitType attackerType, UnitType defenderType);

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
