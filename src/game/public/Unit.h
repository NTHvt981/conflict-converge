#pragma once

#include "raylib.h" // Vector2

#include "MathUtils.h" // cc:: tile-grid snapping (M2 Goal 2)

// Forward-declared API shapes for M3 (Unit System) and M4 (Combat System).
// See plans/MILESTONES.md M3 Unit struct + unit types, M4 damage/armor types.
// No logic here — M3/M4 will flesh out behavior.

// M3: 7 unit types from milestone spec.
enum class UnitType
{
    Infantry,
    AntiArmorInfantry,
    Engineer,
    IFV,
    Artillery,
    LightTank,
    HeavyTank
};

// M4: damage/armor types (inspired by C&C, CoH).
enum class DamageType
{
    KINETIC,
    EXPLOSIVE,
    ENERGY
};

enum class ArmorType
{
    STEEL,
    RUBBER,
    COMPOSITE
};

// M3: unit state machine — Idle -> Moving -> Attacking.
enum class UnitState
{
    Idle,
    Moving,
    Attacking
};

struct Unit
{
    float health = 100.0f;
    ArmorType armorType = ArmorType::STEEL;
    int attackPower = 0;
    int attackRange = 0;
    float cooldown = 0.0f;
    Vector2 position = {}; // snapped to 64x64 grid (M2)
    Vector2 velocity = {};
    bool isSelected = false;
    int teamID = 0;
    UnitType type = UnitType::Infantry;
    UnitState state = UnitState::Idle;
};

// M2 Goal 2: snap a unit's world position to its tile's top-left corner
// (64x64 grid). Units rest on tile corners; pathfinding (M3) moves them
// tile-to-tile, so every stop lands pre-snapped.
inline void SnapUnitToTile(Unit &unit)
{
    unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}
