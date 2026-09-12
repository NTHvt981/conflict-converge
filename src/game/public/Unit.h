#pragma once

#include <cstddef> // std::size_t
#include <vector>  // M3 Goal 3: Unit::path waypoint list

#include "raylib.h" // Vector2

#include "MathUtils.h" // cc:: tile-grid snapping (M2 Goal 2)
#include "Registry.h" // Entity / kInvalidEntity for Unit::target

class TileMap; // movement queries blocked tiles; included in Unit.cpp

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

// M4: attack phases within a single strike. Ready -> WindUp (telegraph, then
// the hit lands through ResolveAttack) -> Recover (rides the cooldown) ->
// Ready. Moving cancels back to Ready (see UpdateUnitMovement).
enum class AttackPhase
{
    Ready,
    WindUp,
    Recover
};

struct Unit
{
    float health = 100.0f;
    ArmorType armorType = ArmorType::STEEL;
    DamageType damageType = DamageType::KINETIC; // dealt by this unit (M4 matrix)
    int attackPower = 0;
    int attackRange = 0; // pixels (circle/radius check, M3G4/M4)
    float cooldown = 0.0f; // live attack timer: seconds until next strike (M3G5)
    float cooldownTime = 0.0f; // seconds between attacks (reset value)
    AttackPhase phase = AttackPhase::Ready; // strike telegraph state (M4G3)
    float phaseTime = 0.0f; // live WindUp countdown (M4G3)
    float windupTime = 0.15f; // telegraph duration before the hit lands (M4G3)
    float speed = 0.0f; // pixels per second (M3G2 stat table)
    float sightRange = 0.0f; // pixels: targeting acquisition radius (M3G4)
    Vector2 position = {}; // snapped to 64x64 grid (M2)
    Vector2 velocity = {};
    bool isSelected = false;
    int teamID = 0;
    UnitType type = UnitType::Infantry;
    UnitState state = UnitState::Idle;
    Entity target = kInvalidEntity; // acquired enemy (M3G4); needs Registry.h
    // M2 Goal 4: single pending move order (tile-snapped destination).
    // A full command queue arrives with M3 AI; M2 moves straight toward
    // the target and stops at the first blocked tile.
    Vector2 moveTarget = {};
    bool hasMoveOrder = false;
    // M3 Goal 3: A* waypoint list (tile indices) with a consumption cursor.
    // Empty/inactive unless hasPath; UpdateUnitMovement walks it waypoint by
    // waypoint and keeps the M2 straight-line behavior otherwise.
    std::vector<cc::IVec2> path;
    std::size_t pathNext = 0;
    bool hasPath = false;
};

// M2 Goal 2: snap a unit's world position to its tile's top-left corner
// (64x64 grid). Units rest on tile corners; pathfinding (M3) moves them
// tile-to-tile, so every stop lands pre-snapped.
inline void SnapUnitToTile(Unit &unit)
{
    unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}

// M2 Goal 4: right-click command input. Stores a tile-snapped destination;
// UpdateUnitMovement (called per frame) walks the unit there.
void IssueMoveOrder(Unit &unit, Vector2 worldTarget);

// Advance one frame toward the pending order; stops snapped on arrival or
// at the first blocked tile (TileMap::IsBlocked, out-of-bounds included).
void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds);

// M3 Goal 5: per-frame AI driver — Idle -> Moving -> Attacking with attack
// cooldowns. Priority: explicit player orders (hasMoveOrder/hasPath) beat AI
// engagement; otherwise the unit acquires (M3G4), chases out-of-range
// targets via path orders, and fires through the M4 damage matrix on
// cooldown when in range. Dead or missing units are skipped (the M3G6
// factory destroys and announces them).
void UpdateUnit(Entity self, Registry &registry, const TileMap &map, float dtSeconds);
