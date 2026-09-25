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
struct Orders;
struct Mover;
struct CombatState;

enum class UnitType
{
    RifleInfantry,
    AntiArmorInfantry,
    Engineer,
    IFV,
    Artillery,
    LightTank,
    HeavyTank,
    PrototypeInfantry,
    Medic,
    Count
};

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

enum class UnitState
{
    Idle,
    Moving,
    Attacking,
    Count
};

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
    Load,
    Unload,
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
    float cooldownTime = 0.0f; // seconds between attacks
    float windupTime = 0.15f; // telegraph before the hit lands
    float speed = 0.0f; // pixels per second
    float sightRange = 0.0f; // pixels
    Vector2 position = {}; // snapped to 64x64 grid
    Vector2 velocity = {};
    bool isSelected = false;
    unsigned int controlGroups = 0; // bit N = control group N; 0 = none
    int teamID = 0;
    UnitType type = UnitType::RifleInfantry;
    UnitState state = UnitState::Idle;
    Facing facing = Facing::Right;
    // Anchor tile is the unit's logical position; the footprint extends
    // toward +x/+y from the anchor. Occupancy and CanEnter check all tiles.
    int footprintWidth = 1;
    int footprintHeight = 1;
};

// Snap a unit's world position to its tile's top-left corner (64x64 grid).
inline void SnapUnitToTile(Unit &unit)
{
    unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}

float EffectiveSpeed(const Unit &unit, const Mover &mover);

void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 worldTarget);

// Call before issuing a fresh non-queued order.
void ClearOrders(Unit &unit, Orders &orders, Mover &mover);

// Attack-move: engage enemies on contact and resume the march when lost.
void IssueAttackMoveOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                          Vector2 worldTarget);

// Footprint-aware attack-move: the initial march routes around units.
void IssueAttackMoveOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                                   const OccupancyGrid &occ, Vector2 worldTarget, Entity self,
                                   std::uint32_t selfGen);

// Leaving Patrol drops the route.
void SetStance(Unit &unit, Orders &orders, CombatState &combat, Stance stance);

void IssuePatrolOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map, Vector2 pointA,
                      Vector2 pointB);

// Engineer repair order on a same-team mechanical unit or Operational building.
void IssueRepairOrder(Unit &engineer, Orders &orders, Mover &mover, Entity target);

// Player-side pre-check for the right-click repair gesture.
bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target);

void IssueHealOrder(Unit &medic, Orders &orders, Mover &mover, Entity target);

// Player-side pre-check for the right-click heal gesture.
bool CanHealTarget(const Registry &registry, const Unit &medic, Entity target);

struct RepairAssignment
{
    Entity engineer = kInvalidEntity;
    Entity target = kInvalidEntity;
};

// Pure sweep of damaged supportable units/buildings overlapping worldArea:
// repairable vehicles + operational buildings (engineers) plus wounded
// flesh units (medics).
void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, int teamID,
                                 std::vector<Entity> &out);

// Never self-pairs.
int AssignAreaRepair(const Registry &registry, const std::vector<Entity> &engineers,
                     const std::vector<Entity> &candidates,
                     std::vector<RepairAssignment> &out);

// Attack-ground: shell worldPos continuously until cancelled.
void IssueAttackGroundOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                            Vector2 worldPos);
void IssueAttackGroundOrderFootprint(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                                     const OccupancyGrid &occ, Vector2 worldPos, Entity self,
                                     std::uint32_t selfGen);

void IssueLoadOrder(Unit &carrier, Orders &orders, Mover &mover, Entity passenger);
void IssueUnloadOrder(Unit &carrier, Orders &orders, Mover &mover, const TileMap &map,
                      Vector2 worldPos);
// Foot-only, same-team, live, non-embarked passenger with a free seat.
// Enemies/vehicles/full carriers reject like heal validation.
bool CanLoadTarget(const Registry &registry, Entity carrier, const Unit &carrierUnit,
                   Entity passenger);
// False when invalid.
bool BoardTransport(Registry &registry, Entity carrier, Entity passenger);
int UnloadTransport(Registry &registry, const TileMap &map, Entity carrier, Vector2 worldPos);

// Shared retreat threshold: fraction of type-max HP below which units fall back.
inline constexpr float kRetreatHealthFraction = 0.3f;

// Resolve the player's retreat home: rally point, else nearest owned Base.
Vector2 ResolvePlayerRetreatHome(Registry &registry, Vector2 rallyPos);

void RetreatIfLowHP(Registry &registry, TileMap &map, OccupancyGrid *occ, Vector2 home, int teamID,
                    float healthFraction, bool onlyAutoRetreat);

void IssueOrEnqueue(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
                    OccupancyGrid *occ, Entity self, std::uint32_t selfGen, bool shiftQueue,
                    QueuedOrder order);

// Stops snapped on arrival.
void UpdateUnitMovement(Unit &unit, Orders &orders, Mover &mover, CombatState &combat,
                        const TileMap &map, float speedPixelsPerSec, float dtSeconds,
                        OccupancyGrid *occ = nullptr, Entity self = 0,
                        std::uint32_t selfGen = 0);

void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
                const FogOfWar *fog = nullptr, OccupancyGrid *occ = nullptr,
                const std::unordered_map<Entity, float> *reserved = nullptr);

void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ);

// G3 crush: a `crushesFlesh` unit overlapping an enemy foot unit kills it
// instead of pushing. Team-checked (no friendly crush); kills resolve before
// occupancy so the tile frees. Vehicles/buildings are never crushed.
void ResolveCrush(Registry &registry);

void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
                          const FogOfWar *fog, float dtSeconds);
