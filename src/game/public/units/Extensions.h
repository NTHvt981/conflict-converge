#pragma once

#include <cstddef>
#include <vector>

#include "units/Combat.h"
#include "core/Registry.h"
#include "core/Team.h"
#include "economy/Nodes.h"
#include "units/Unit.h"

// Header-only: no TU, no premake regen.

template <typename T> T &GetOrAdd(Registry &registry, Entity entity)
{
	if (T *existing = registry.Get<T>(entity))
	{
		return *existing;
	}
	registry.Add(entity, T{});
	return *registry.Get<T>(entity);
}

struct Turret
{
	float facing = 0.0f;   // radians, world-space
	float turnRate = 0.0f; // radians per second
};

struct Cargo
{
	int capacity = 0;
	std::vector<Entity> passengers;
};

enum class HarvestPhase
{
	ToField,
	Loading,
	ToDepot,
	Unloading
};

struct Harvester
{
	HarvestPhase phase = HarvestPhase::ToField;
	float load = 0.0f;
	float capacity = 100.0f;
	ResourceKind loadKind = ResourceKind::Iron;
	cc::IVec2 nodeTile{ -1, -1 };
};

inline Harvester &GetHarvester(Registry &registry, Entity entity)
{
	return GetOrAdd<Harvester>(registry, entity);
}

struct EmbarkedOn
{
	Entity carrier = kInvalidEntity;
};

struct Projectile
{
	Vector2 start = {};      // launch point snapshot
	Vector2 target = {};     // landing snapshot; units can move out of it
	float flightTime = 0.0f; // seconds
	float elapsed = 0.0f;
	int attackPower = 0;
	DamageType damageType = DamageType::KINETIC;
	TeamId teamID = kPlayerTeam;
	int splashTiles = 0; // AoE half-extent = splashTiles * 32px
};

inline bool IsEmbarked(const Registry &registry, Entity entity)
{
	return registry.Has<EmbarkedOn>(entity);
}

inline bool IsFleshUnit(UnitType type)
{
	return !IsVehicleHull(type);
}

struct Orders
{
	Stance stance = Stance::Guard;
	bool autoRetreat = false;
	bool attackMove = false;
	Vector2 attackMoveDest = {};
	bool hasPatrol = false;
	Vector2 patrolA = {};
	Vector2 patrolB = {};
	bool patrolToB = true;
	bool hasRepairOrder = false;
	Entity repairTarget = kInvalidEntity;
	bool autoRepair = false;
	bool hasAttackGroundOrder = false;
	Vector2 attackGroundPos = {};
	bool hasLoadOrder = false;
	Entity loadTarget = kInvalidEntity;
	bool hasUnloadOrder = false;
	Vector2 unloadPos = {}; // tile-snapped drop point
	std::vector<QueuedOrder> orderQueue; // pending orders, FIFO
};

inline Orders &GetOrders(Registry &registry, Entity entity)
{
	return GetOrAdd<Orders>(registry, entity);
}

struct Mover
{
	Vector2 moveTarget = {}; // tile-snapped destination
	bool hasMoveOrder = false;
	std::vector<cc::IVec2> path; // tile-index waypoints
	std::size_t pathNext = 0;
	bool hasPath = false;
	float blockedTime = 0.0f;
	int blockedRepaths = 0;
	float speedCapPixelsPerSec = -1.0f; // slowest-speed cap; -1 = uncapped
	Entity yieldRequestBlocker = kInvalidEntity;
	cc::IVec2 yieldRequestTile{ -1, -1 };
	Entity awaitingClearBlocker = kInvalidEntity;
	cc::IVec2 awaitingClearTile{ -1, -1 };
	float awaitingClearTime = 0.0f;
	bool yielding = false;
	Entity yieldDeclined = kInvalidEntity;
};

inline Mover &GetMover(Registry &registry, Entity entity)
{
	return GetOrAdd<Mover>(registry, entity);
}

inline float EffectiveSpeed(const Unit &unit, const Mover &mover)
{
	if (mover.speedCapPixelsPerSec < 0.0f || mover.speedCapPixelsPerSec >= unit.speed)
	{
		return unit.speed;
	}
	return mover.speedCapPixelsPerSec;
}

struct CombatState
{
	Entity target = kInvalidEntity;
	AttackPhase phase = AttackPhase::Ready;
	float phaseTime = 0.0f; // WindUp countdown
	float cooldown = 0.0f; // seconds to next strike
	float lastDamageTaken = 0.0f;
	float hitFlashTime = 0.0f;
};

inline CombatState &GetCombatState(Registry &registry, Entity entity)
{
	return GetOrAdd<CombatState>(registry, entity);
}

// Corpse linger timer for the death animation; never saved.
struct DeathFx
{
	float timeLeft = 0.0f;
};
