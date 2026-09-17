#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // QoL reserved-damage map (see Targeting.h alias)
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

// M3: 7 unit types from milestone spec, plus the prototype-sandbox type
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
    Count // keep last: save decode validates < Count (see SaveGame.cpp)
};

// M4: damage/armor types (inspired by C&C, CoH).
enum class DamageType
{
    KINETIC,
    EXPLOSIVE,
    ENERGY,
    Count // keep last: save decode validates < Count
};

enum class ArmorType
{
    STEEL,
    RUBBER,
    COMPOSITE,
    Count // keep last: save decode validates < Count
};

// M3: unit state machine — Idle -> Moving -> Attacking.
enum class UnitState
{
    Idle,
    Moving,
    Attacking,
    Count // keep last: save decode validates < Count
};

// M4: attack phases within a single strike. Ready -> WindUp (telegraph, then
// the hit lands through ResolveAttack) -> Recover (rides the cooldown) ->
// Ready. Moving cancels back to Ready (see UpdateUnitMovement).
enum class AttackPhase
{
    Ready,
    WindUp,
    Recover,
    Count // keep last: save decode validates < Count
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

// QoL shift-queued order. `kind` selects which payload fields are valid;
// unused fields are ignored (Patrol uses pointA/pointB, Move uses only
// pointA, Repair uses target). Keep Count last (matches the save-enum
// convention) so future order types extend the same dispatch.
enum class QueuedOrderKind
{
    Move,
    AttackMove,
    Patrol,
    Repair,
    AttackGround, // reserved: needs the attack-ground order type first
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
// Stored on Unit, maintained by the movement driver; render-only
// consumers (atlas lookup, editor preview) read it, nothing else.
// Transient like controlGroups: not saved, defaults to Right on load.
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

// Screen-space velocity (y down) -> nearest octant column. Zero vector
// returns Right (deterministic; callers only invoke it while stepping).
Facing FacingFromVelocity(Vector2 velocity);

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
    // QoL move-at-slowest-speed: caps effective speed below Unit::speed for
    // the current order so fast units don't outrun slow ones in formation.
    // -1 = uncapped. Reset on every new order, arrival, and cancel (see
    // ClearOrders/Arrive/CancelAtBlocked/Space) so it never leaks into an
    // unrelated later order.
    float speedCapPixelsPerSec = -1.0f;
    Vector2 position = {}; // snapped to 64x64 grid (M2)
    Vector2 velocity = {};
    bool isSelected = false;
    // QoL control groups: bit N = member of group N (10 groups, 0-9).
    // 0 = none. Transient UX convenience like isSelected: not saved.
    // Assigned via Ctrl+number (replace), extended via Shift+number (add),
    // recalled via plain number.
    unsigned int controlGroups = 0;
    int teamID = 0;
    UnitType type = UnitType::Infantry;
    UnitState state = UnitState::Idle;
    // Atlas facing (prototype directional sheets). Last travel direction;
    // idle/attacking units keep it. Render-only: not saved, not simulated.
    Facing facing = Facing::Right;
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
    // QoL auto-retreat opt-in (player side; AI retreats by difficulty and
    // ignores this flag — see RetreatIfLowHP's onlyAutoRetreat parameter).
    bool autoRetreat = false;
    bool hasPatrol = false;
    Vector2 patrolA = {};
    Vector2 patrolB = {};
    bool patrolToB = true;
    // M13: Engineer repair order (channeled, time cost only — Q83).
    bool hasRepairOrder = false;
    Entity repairTarget = kInvalidEntity;
    // QoL attack-ground (standing shell-at-position order until cancelled).
    // Continuous: keeps firing on cooldown while in range, hits nothing when
    // the impact area is empty. Cleared like every other order below.
    bool hasAttackGroundOrder = false;
    Vector2 attackGroundPos = {};
    // QoL shift-queue: pending orders behind the current one, dispatched in
    // FIFO order as each completes. Transient like the rest of the order
    // state: not saved. Patrol never completes (loops), so anything queued
    // behind a patrol runs only if the patrol is explicitly overwritten.
    std::vector<QueuedOrder> orderQueue;
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
    // Stall-fix: same wait/replan/cancel budget as blockedTime/blockedRepaths
    // above, but tracked separately for ReportSeparationStall. A unit stuck
    // in the SeparateUnits equilibrium keeps reporting StepResult::Stepped
    // every frame (it never crosses a tile boundary), which unconditionally
    // resets blockedTime to 0 -- sharing counters would erase this budget
    // before it could ever reach the retry threshold. Reset alongside
    // blockedTime/blockedRepaths by every new order, arrival, and cancel.
    float separationStallTime = 0.0f;
    int separationStallRepaths = 0;
};

// M2 Goal 2: snap a unit's world position to its tile's top-left corner
// (64x64 grid). Units rest on tile corners; pathfinding (M3) moves them
// tile-to-tile, so every stop lands pre-snapped.
inline void SnapUnitToTile(Unit &unit)
{
    unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}

// QoL: effective movement speed honoring the slowest-speed cap (-1 = own
// speed). All UpdateUnitMovement call sites use this, never Unit::speed.
inline float EffectiveSpeed(const Unit &unit)
{
    if (unit.speedCapPixelsPerSec < 0.0f || unit.speedCapPixelsPerSec >= unit.speed)
    {
        return unit.speed;
    }
    return unit.speedCapPixelsPerSec;
}

// M2 Goal 4: right-click command input. Stores a tile-snapped destination;
// UpdateUnitMovement (called per frame) walks the unit there.
void IssueMoveOrder(Unit &unit, Vector2 worldTarget);

// Shared clear: exactly one order active at a time. Every Issue*Order
// wrapper calls this before setting its own flag. Call it directly
// (before IssuePathOrderFootprint / formation::Issue*FormationMoveFP)
// wherever a fresh, non-queued order bypasses those wrappers -- e.g. the
// direct mouse-order dispatch in Game.cpp -- so a leftover repair/
// attack-ground/patrol flag can't silently swallow the new order (see
// plans/Bugfix_Order_Flags_And_Retreat_Fallback_Plan.md).
void ClearOrders(Unit &unit);

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

// QoL: player-side pre-check for the right-click repair gesture. True when
// issuing a repair order on `target` would stick (same validation the
// driver runs, without starting the order).
bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target);

// QoL area repair: one greedy assignment (issuing Engineer + target).
struct RepairAssignment
{
    Entity engineer = kInvalidEntity;
    Entity target = kInvalidEntity;
};

// QoL area repair candidate sweep (pure query, no orders): damaged
// same-team repairable mechanical units below max HP + Operational
// buildings below maxHealth whose footprint overlaps worldArea.
void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, int teamID,
                                 std::vector<Entity> &out);

// QoL area repair assignment (pure, no orders issued): each engineer takes
// its nearest still-unclaimed candidate passing CanRepairTarget (greedy
// bestDistSq scan against a mutable claimed set). Excess engineers keep
// their current orders; excess candidates wait for the next drag.
int AssignAreaRepair(const Registry &registry, const std::vector<Entity> &engineers,
                     const std::vector<Entity> &candidates,
                     std::vector<RepairAssignment> &out);

// QoL attack-ground order: shell `worldPos` continuously until cancelled.
// Out of range, the unit marches there first (footprint-aware variant
// available); in range it fires through the normal windup/cooldown machine,
// hitting nothing when the impact area is empty.
void IssueAttackGroundOrder(Unit &unit, const TileMap &map, Vector2 worldPos);
void IssueAttackGroundOrderFootprint(Unit &unit, const TileMap &map, const OccupancyGrid &occ,
                                     Vector2 worldPos, Entity self, std::uint32_t selfGen);

// QoL + AI shared retreat threshold: fraction of type-max HP below which
// damaged units fall back. Hoisted from AICommander::RetreatTick so both
// call sites reference one named constant (tune here, not inline).
inline constexpr float kRetreatHealthFraction = 0.3f;

// QoL player auto-retreat fallback: the rally point when one was ever
// placed, otherwise the owned Operational Base nearest the centroid of
// the player's own living units -- never nearest the map's center (see
// plans/Bugfix_Order_Flags_And_Retreat_Fallback_Plan.md). Pure
// home-resolution for the Game.cpp retreat pass, so the multi-base case
// is unit-testable; RetreatIfLowHP stays the shared per-unit driver for
// both player and AI (its signature is untouched).
Vector2 ResolvePlayerRetreatHome(Registry &registry, Vector2 rallyPos);

// Orders sub-threshold, non-Engineer, alive units of teamID to fall back
// toward `home` via fighting withdrawal (attack-move, never plain move —
// plain move forfeits full-DPS units, measured in the Medium-vs-Hard soak).
// onlyAutoRetreat=true additionally requires Unit::autoRetreat (player
// opt-in); the AI passes false (its units retreat by difficulty instead).
void RetreatIfLowHP(Registry &registry, TileMap &map, OccupancyGrid *occ, Vector2 home, int teamID,
                    float healthFraction, bool onlyAutoRetreat);

// QoL shift-queue entry point. shiftQueue=false: clears orderQueue and
// issues immediately through the same clean dispatch as dequeued orders
// (exactly one active order; footprint-aware where applicable).
// shiftQueue=true: appends instead — unless the unit is fully idle with an
// empty queue, in which case it issues immediately (queueing on an idle
// unit starts it right away, matching genre convention). Used by the
// keyboard order shortcuts; plain right-click keeps its legacy direct
// IssuePathOrderFootprint call (plus a queue clear) and only routes
// Shift+right-click through here.
void IssueOrEnqueue(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                    std::uint32_t selfGen, bool shiftQueue, QueuedOrder order);

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
// QoL: pass the frame's reserved-damage map for overkill protection
// (Targeting.h's ReservedDamageMap; nullptr = legacy, tests keep working).
void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
                const FogOfWar *fog = nullptr, OccupancyGrid *occ = nullptr,
                const std::unordered_map<Entity, float> *reserved = nullptr);

// Stall-fix: SeparateUnits' continuous-space push can undo a step that
// StepToward already reported as successful (Stepped, non-zero velocity),
// wedging two converging units apart just short of the tile boundary that
// CanEnter would have gated. StepToward/TryBlockedRetry never see this --
// nothing ever tried to enter an occupied tile -- so it needs its own hook.
// Applies the same wait/replan/cancel budget as a normal transient
// unit-block (TryBlockedRetry). No-op unless the unit has an active order.
void ReportSeparationStall(Unit &unit, const TileMap &map, OccupancyGrid *occ, Entity self,
                           std::uint32_t selfGen, float dtSeconds);

// Full per-frame movement pipeline: occupancy pre-pass, per-unit AI/movement
// driver, continuous-space separation, then separation-stall detection.
// Extracted so the real game loop and tests drive units through the exact
// same sequence and can't drift apart on ordering (see ReportSeparationStall).
void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
                          const FogOfWar *fog, float dtSeconds);
