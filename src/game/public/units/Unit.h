#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "raylib.h"

#include "core/MathUtils.h"
#include "core/Registry.h"
#include "core/Team.h"

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

enum class Stance
{
	Hold,
	Guard,
	Patrol,
	Count
};

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
	Vector2 pointB = {};
	Entity target = kInvalidEntity;
};

// Atlas column: 0 top, then counter-clockwise to 7 top-right.
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

// Nearest octant from screen velocity (y down); zero -> Right.
Facing FacingFromVelocity(Vector2 velocity);

struct Unit
{
	float health = 100.0f;
	ArmorType armorType = ArmorType::STEEL;
	DamageType damageType = DamageType::KINETIC; // dealt by this unit
	int attackPower = 0;
	int attackRange = 0; // pixels
	float cooldownTime = 0.0f; // seconds
	float windupTime = 0.15f;
	float speed = 0.0f; // pixels per second
	float sightRange = 0.0f; // pixels
	Vector2 position = {}; // snapped to the tile grid
	Vector2 velocity = {};
	bool isSelected = false;
	unsigned int controlGroups = 0;
	TeamId teamID = kPlayerTeam;
	UnitType type = UnitType::RifleInfantry;
	UnitState state = UnitState::Idle;
	Facing facing = Facing::Right;
	// Anchor tile is the logical position; the footprint extends toward
	// +x/+y from it, and occupancy checks all covered tiles.
	int footprintWidth = 1;
	int footprintHeight = 1;
};

// Top-left corner of the unit's tile.
inline void SnapUnitToTile(Unit &unit)
{
	unit.position = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(unit.position)));
}

struct OrderContext
{
	Orders &orders;
	Mover &mover;
	const TileMap &map;
	const OccupancyGrid *occ = nullptr;
	Entity self = kInvalidEntity;
	std::uint32_t selfGen = 0;
};

OrderContext MakeOrderContext(Registry &registry, Entity self, const TileMap &map,
							  const OccupancyGrid *occ);

float EffectiveSpeed(const Unit &unit, const Mover &mover);

void IssueMoveOrder(Unit &unit, Orders &orders, Mover &mover, Vector2 worldTarget);

void ClearOrders(Unit &unit, Orders &orders, Mover &mover);

void IssueAttackMoveOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
						  Vector2 worldTarget);

void IssueAttackMoveOrderFootprint(Unit &unit, const OrderContext &ctx, Vector2 worldTarget);

// Leaving Patrol drops the route.
void SetStance(Unit &unit, Orders &orders, CombatState &combat, Stance stance);

void IssuePatrolOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map, Vector2 pointA,
					  Vector2 pointB);

// Same-team mechanical unit or Operational building.
void IssueRepairOrder(Unit &engineer, Orders &orders, Mover &mover, Entity target);

bool CanRepairTarget(const Registry &registry, const Unit &engineer, Entity target);

void IssueHealOrder(Unit &medic, Orders &orders, Mover &mover, Entity target);

bool CanHealTarget(const Registry &registry, const Unit &medic, Entity target);

struct RepairAssignment
{
	Entity engineer = kInvalidEntity;
	Entity target = kInvalidEntity;
};

// Repairable vehicles + operational buildings, plus wounded flesh units (medics).
void CollectAreaRepairCandidates(Registry &registry, Rectangle worldArea, TeamId teamID,
								 std::vector<Entity> &out);

// Never self-pairs.
int AssignAreaRepair(const Registry &registry, const std::vector<Entity> &engineers,
					 const std::vector<Entity> &candidates,
					 std::vector<RepairAssignment> &out);

// Attack-ground: shell worldPos until cancelled.
void IssueAttackGroundOrder(Unit &unit, Orders &orders, Mover &mover, const TileMap &map,
							Vector2 worldPos);
void IssueAttackGroundOrderFootprint(Unit &unit, const OrderContext &ctx, Vector2 worldPos);

void IssueLoadOrder(Unit &carrier, Orders &orders, Mover &mover, Entity passenger);
void IssueUnloadOrder(Unit &carrier, Orders &orders, Mover &mover, const TileMap &map,
					  Vector2 worldPos);
bool CanLoadTarget(const Registry &registry, Entity carrier, const Unit &carrierUnit,
				   Entity passenger);
// False when invalid.
bool BoardTransport(Registry &registry, Entity carrier, Entity passenger);
int UnloadTransport(Registry &registry, const TileMap &map, Entity carrier, Vector2 worldPos);

// Fraction of type-max HP; shared by player and AI.
inline constexpr float kRetreatHealthFraction = 0.3f;

Vector2 ResolvePlayerRetreatHome(Registry &registry);

void RetreatIfLowHP(Registry &registry, TileMap &map, OccupancyGrid *occ, Vector2 home,
					TeamId teamID, float healthFraction, bool onlyAutoRetreat);

void IssueOrEnqueue(Unit &unit, const OrderContext &ctx, bool shiftQueue, QueuedOrder order);

// Stops snapped on arrival.
void UpdateUnitMovement(Unit &unit, const OrderContext &ctx, CombatState &combat,
						float speedPixelsPerSec, float dtSeconds);

void UpdateUnit(Entity self, Registry &registry, TileMap &map, float dtSeconds,
				const FogOfWar *fog = nullptr, OccupancyGrid *occ = nullptr,
				const std::unordered_map<Entity, float> *reserved = nullptr);

void ResolveStackedUnits(Registry &registry, const TileMap &map, OccupancyGrid &occ);

void PumpYieldHandshake(Registry &registry, const TileMap &map, OccupancyGrid &occ,
						float dtSeconds);

void ResolveCrush(Registry &registry);

void RunUnitMovementFrame(Registry &registry, TileMap &map, OccupancyGrid &occ,
						  const FogOfWar *fog, float dtSeconds);
