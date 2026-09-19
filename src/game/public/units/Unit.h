#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "raylib.h"

#include "MathUtils.h"
#include "Registry.h"

class TileMap;
class OccupancyGrid;
class FogOfWar;

// 7 unit types plus the prototype-sandbox type
// (prototype art at 2x; sandbox levels only, never in factory menus).
enum class UnitType
{
    Infantry,
    AntiArmorInfantry,
    Engineer,
    IFV,
    Artillery,
    LightTank,
    HeavyTank,
    PrototypeInfantry,
    Count
};

// Damage/armor types (inspired by C&C, CoH).
enum class DamageType
{
    KINETIC,
    EXPLOSIVE,
    ENERGY,
    Count
};

enum class ArmorType
{
    STEEL,
    RUBBER,
    COMPOSITE,
    Count
};

// Unit state machine — Idle -> Moving -> Attacking.
enum class UnitState
{
    Idle,
    Moving,
    Attacking,
    Count
};

// Attack phases within a single strike: Ready -> WindUp -> Recover -> Ready.
enum class AttackPhase
{
    Ready,
    WindUp,
    Recover,
    Count
};

// Combat stances: Guard acquires and chases, Hold stands and fires,
// Patrol loops waypoints when no combat interrupts.
enum class Stance
{
    Hold,
    Guard,
    Patrol
};

// Shift-queued order; `kind` selects which payload fields are valid.
enum class QueuedOrderKind
{
    Move,
    AttackMove,
    Patrol,
    Repair,
    AttackGround,
    Count
};

struct QueuedOrder
{
    QueuedOrderKind kind = QueuedOrderKind::Move;
    Vector2 pointA = {};
    Vector2 pointB = {}; // Patrol's second waypoint
    Entity target = kInvalidEntity; // Repair's target
};

// 8-way facing = prototype sheet column: 0 top, 1 top-left, 2 left,
// 3 bottom-left, 4 bottom, 5 bottom-right, 6 right, 7 top-right.
enum class Facing : int
{
    Top = 0,
    TopLeft = 1,
    Left = 2,
    BottomLeft = 3,
    Bottom = 4,
    BottomRight = 5,
    Right = 6,
    TopRight = 7,
    Count
};

// Screen-space velocity (y down) -> nearest octant column; zero -> Right.
Facing FacingFromVelocity(Vector2 velocity);

struct Unit
{
    float health = 100.0f;
    ArmorType armorType = ArmorType::STEEL;
    DamageType damageType = DamageType::KINETIC; // dealt by this unit
    int attackPower = 0;
    int attackRange = 0; // pixels
    float cooldown = 0.0f; // seconds until next strike
    float cooldownTime = 0.0f; // seconds between attacks
    AttackPhase phase = AttackPhase::Ready;
    float phaseTime = 0.0f; // live WindUp countdown
    float windupTime = 0.15f; // telegraph before the hit lands
    float lastDamageTaken = 0.0f; // most recent effective hit
    float hitFlashTime = 0.0f; // live hit-overlay countdown
    float speed = 0.0f; // pixels per second
    float sightRange = 0.0f; // pixels
    float speedCapPixelsPerSec = -1.0f; // slowest-speed cap; -1 = uncapped
    Vector2 position = {}; // snapped to 64x64 grid
    Vector2 velocity = {};
    bool isSelected = false;
    unsigned int controlGroups = 0; // bit N = control group N; 0 = none
    int teamID = 0;
    UnitType type = UnitType::Infantry;
    UnitState state = UnitState::Idle;
    Facing facing = Facing::Right; // last travel direction
    Entity target = kInvalidEntity; // acquired enemy
    Vector2 moveTarget = {}; // tile-snapped pending move destination
    bool hasMoveOrder = false;
    std::vector<cc::IVec2> path; // A* waypoints (tile indices)
    std::size_t pathNext = 0;
    bool hasPath = false;
    bool attackMove = false;
    Vector2 attackMoveDest = {}; // march goal kept across chase detours
    Stance stance = Stance::Guard;
    bool autoRetreat = false; // player opt-in auto-retreat
    bool hasPatrol = false;
    Vector2 patrolA = {};
    Vector2 patrolB = {};
    bool patrolToB = true;
    bool hasRepairOrder = false; // channeled Engineer repair
    Entity repairTarget = kInvalidEntity;
    bool hasAttackGroundOrder = false;
    Vector2 attackGroundPos = {};
    std::vector<QueuedOrder> orderQueue; // pending orders, dispatched FIFO
    // Multi-tile footprint. 1x1 for infantry, 2x2 for vehicles.
    // Anchor tile is the unit's logical position; the footprint extends
    // toward +x/+y from the anchor. Occupancy and CanEnter check all tiles.
    int footprintWidth = 1;
    int footprintHeight = 1;
    float blockedTime = 0.0f; // blocked-move retry accounting
    int blockedRepaths = 0;
};

// Snap a unit's world position to its tile's top-left corner (64x64 grid).
inline void SnapUnitToTile(Unit &unit)
{
    unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}

// Effective movement speed honoring the slowest-speed cap (-1 = own speed).
inline float EffectiveSpeed(const Unit &unit)
{
    if (unit.speedCapPixelsPerSec < 0.0f || unit.speedCapPixelsPerSec >= unit.speed)
    {
        return unit.speed;
    }
    return unit.speedCapPixelsPerSec;
}

// Issue a tile-snapped move order; UpdateUnitMovement walks it per frame.
void IssueMoveOrder(Unit &unit, Vector2 worldTarget);

// Clear every active order; call before issuing a fresh non-queued order.
void ClearOrders(Unit &unit);

// Attack-move: engage enemies on contact and resume the march when lost.
void IssueAttackMoveOrder(Unit &unit, const TileMap &map, Vector2 worldTarget);

// Footprint-aware attack-move: the initial march routes around units.
void IssueAttackMoveOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                   Vector2 worldTarget, Entity self, std::uint32_t selfGen);

// Switch stance; leaving Patrol drops the route.
void SetStance(Unit &unit, Stance stance);

// Patrol between two world points, looping while idle.
void IssuePatrolOrder(Unit &unit, const TileMap &map, Vector2 pointA, Vector2 pointB);

// Engineer repair order on a same-team mechanical unit or Operational building.
void IssueRepairOrder(Unit &engineer, Entity target);

// Player-side pre-check for the right-click repair gesture.
bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target);

// One greedy area-repair pairing (engineer + target).
struct RepairAssignment
{
    Entity engineer = kInvalidEntity;
    Entity target = kInvalidEntity;
};

// Pure sweep of damaged repairable units/buildings overlapping worldArea.
void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, int teamID,
                                 std::vector<Entity> &out);

// Pure greedy assignment of engineers to nearest unclaimed candidates.
int AssignAreaRepair(const Registry &registry, const std::vector<Entity> &engineers,
                     const std::vector<Entity> &candidates,
                     std::vector<RepairAssignment> &out);

// Attack-ground: shell worldPos continuously until cancelled.
void IssueAttackGroundOrder(Unit &unit, const TileMap &map, Vector2 worldPos);
void IssueAttackGroundOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                     Vector2 worldPos, Entity self, std::uint32_t selfGen);

// Shared retreat threshold: fraction of type-max HP below which units fall back.
inline constexpr float kRetreatHealthFraction = 0.3f;

// Resolve the player's retreat home: rally point, else nearest owned Base.
Vector2 ResolvePlayerRetreatHome(Registry &registry, Vector2 rallyPos);

// Order sub-threshold units of teamID to fall back via fighting withdrawal.
void RetreatIfLowHP(Registry &registry, TileMap &map, OccupancyGrid *occ, Vector2 home, int teamID,
                    float healthFraction, bool onlyAutoRetreat);

// Issue immediately, or append to the shift-queue.
void IssueOrEnqueue(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                    std::uint32_t selfGen, bool shiftQueue, QueuedOrder order);

// Advance one frame toward the pending order; stops snapped on arrival.
void UpdateUnitMovement(Unit &unit, const TileMap &map, float speedPixelsPerSec, float dtSeconds,
                        OccupancyGrid *occ = nullptr, Entity self = 0,
                        std::uint32_t selfGen = 0);

// Per-frame AI driver: acquire, chase, and attack on cooldown.
void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
                const FogOfWar *fog = nullptr, OccupancyGrid *occ = nullptr,
                const std::unordered_map<Entity, float> *reserved = nullptr);

// Relocate one unit per exact-tile stack to a reachable neighbouring tile.
void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ);

// Full per-frame movement pipeline: occupancy pre-pass, driver, stack relocation.
void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
                          const FogOfWar *fog, float dtSeconds);
