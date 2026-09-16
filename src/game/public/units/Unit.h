#pragma once

#include <cstddef> // std::size_t
#include <vector>  // M3 Goal 3: Unit::path waypoint list

#include "raylib.h" // Vector2

#include "MathUtils.h" // cc:: tile-grid snapping (M2 Goal 2)
#include "Registry.h" // Entity / kInvalidEntity for Unit::target

class TileMap; // movement queries blocked tiles; included in Unit.cpp
class OccupancyGrid; // Phase 4: footprint-aware movement; included in Unit.cpp
class FogOfWar;  // M9 visibility gate for acquisition; included in Unit.cpp

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

// M13: combat stances. Guard is the legacy behavior (acquire + chase);
// Hold stands still but fires at in-range enemies; Patrol loops waypoints
// when no combat interrupts.
enum class Stance
{
    Hold,
    Guard,
    Patrol
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
    float lastDamageTaken = 0.0f; // most recent effective hit, for the M4G5 number
    float hitFlashTime = 0.0f;    // live overlay countdown, decayed in UpdateUnit (M4G5)
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
    // M13: attack-move (engage on contact, resume path after). moveTarget
    // carries the march goal while attackMoveDest remembers it across
    // chase detours; the driver re-issues when the two diverge.
    bool attackMove = false;
    Vector2 attackMoveDest = {};
    // M13: stance + patrol route (looping waypoint pair while idle).
    Stance stance = Stance::Guard;
    bool hasPatrol = false;
    Vector2 patrolA = {};
    Vector2 patrolB = {};
    bool patrolToB = true;
    // M13: Engineer repair order (channeled, time cost only — Q83).
    bool hasRepairOrder = false;
    Entity repairTarget = kInvalidEntity;
    // Phase 4: multi-tile footprint. 1x1 for infantry, 2x2 for vehicles.
    // Anchor tile is the unit's logical position; the footprint extends
    // toward +x/+y from the anchor. Occupancy and CanEnter check all tiles.
    int footprintWidth = 1;
    int footprintHeight = 1;
    // Blocked-move retry (transient, never saved): when a step is blocked by
    // another unit, the order is kept and the path recomputed instead of
    // instantly cancelled. blockedTime accrues while blocked; every retry
    // interval the route is replanned; after the repath budget runs out the
    // order cancels as before. Reset by every new order, arrival, and cancel.
    float blockedTime = 0.0f;
    int blockedRepaths = 0;
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

// M13: attack-move order. Like a move order, but the driver engages enemies
// on contact and resumes the march when the target is lost.
void IssueAttackMoveOrder(Unit &unit, const TileMap &map, Vector2 worldTarget);

// Attack-move with a footprint-aware march route (8-dir A* + CanEnter +
// occupied-goal sanitization). Chase/remarch legs stay driver-issued as
// before; only the initial march plans around units.
void IssueAttackMoveOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                   Vector2 worldTarget, Entity self, std::uint32_t selfGen);

// M13: stance switch. Leaving Patrol drops the route; orders are untouched.
void SetStance(Unit &unit, Stance stance);

// M13: patrol route between two world points (snapped). The driver loops
// the legs while idle with no combat to answer.
void IssuePatrolOrder(Unit &unit, const TileMap &map, Vector2 pointA, Vector2 pointB);

// M13: Engineer repair order on a same-team mechanical unit or Operational
// building. Heals over time while in range; costs time, not resources (Q83).
// No-op unless the issuer is an Engineer.
void IssueRepairOrder(Unit &engineer, Entity target);

// Advance one frame toward the pending order; stops snapped on arrival.
// Terrain-blocked steps cancel the order immediately (M2 legacy); steps
// blocked by another unit wait and replan a few times first, cancelling
// only when the retry budget runs out.
// Phase 4: when occ is non-null, checks footprint occupancy to prevent
// stepping into occupied tiles; entity/generation identify self.
void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds,
                        OccupancyGrid *occ = nullptr, Entity self = 0,
                        std::uint32_t selfGen = 0);

// Overlap avoidance pass: pushes living units whose footprint-sized bodies
// intersect apart (half-overlap each, speed-capped per frame).
// Deterministic (no RNG) so tests can assert exact spreads. Call once per
// frame after the UpdateUnit loop; corpses are ignored.
void SeparateUnits(Registry &registry, float dtSeconds);

// M3 Goal 5: per-frame AI driver — Idle -> Moving -> Attacking with attack
// cooldowns. Priority: explicit player orders (hasMoveOrder/hasPath) beat AI
// engagement; otherwise the unit acquires (M3G4), chases out-of-range
// targets via path orders, and fires through the M4 damage matrix on
// cooldown when in range. Dead or missing units are skipped (the M3G6
// factory destroys and announces them).
// M9: pass fog to gate acquisition + chase validation on visibility
// (nullptr = ungated legacy behavior, keeps old call sites working).
// Phase 4: pass occ for footprint-aware movement (nullptr = legacy behavior).
void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
                const FogOfWar *fog = nullptr, OccupancyGrid *occ = nullptr);
