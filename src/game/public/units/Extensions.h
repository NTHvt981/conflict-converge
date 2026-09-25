#pragma once

#include <cstddef>
#include <vector>

#include "Combat.h"
#include "Registry.h"
#include "Unit.h"

// M2 extension components: gimmick state lives in Registry pools, not in the
// flat Unit struct. Units without gimmicks never get them; existing systems
// skip them by pool (see IsEmbarked). Header-only: no TU, no premake regen.

struct Turret
{
    float facing = 0.0f;   // radians, world-space aim direction
    float turnRate = 0.0f; // radians per second (from UnitConfig abilities)
};

struct Cargo
{
    int capacity = 0;
    std::vector<Entity> passengers;
};

struct EmbarkedOn
{
    Entity carrier = kInvalidEntity;
};

struct Projectile
{
    Vector2 start = {};    // launch point (attacker center snapshot)
    Vector2 target = {};   // landing point snapshot (dodgeable: units may move)
    float flightTime = 0.0f; // total seconds aloft
    float elapsed = 0.0f;
    int attackPower = 0;
    DamageType damageType = DamageType::KINETIC;
    int teamID = 0;
    int splashTiles = 0; // AoE half-extent = splashTiles * 32px
};

// True when the entity rides inside a carrier: movement, targeting,
// separation, and rendering skip it; its state syncs on unload.
inline bool IsEmbarked(const Registry &registry, Entity entity)
{
    return registry.Has<EmbarkedOn>(entity);
}

// Flesh predicate (foot units): crush victims, transport passengers, heal
// targets. Vehicles (hulls) are never flesh.
inline bool IsFleshUnit(UnitType type)
{
    return !IsVehicleHull(type);
}

// --- Core behavior components (Unit split, slice 1: orders) ---
// Order intent lives here, not on Unit: stance/attack-move/patrol/repair/
// attack-ground/load/unload state plus the shift-queue. Attached at spawn
// for every unit; GetOrders creates a default on first use so callers never
// branch on missing. Movement execution (hasMoveOrder/hasPath/path) moved
// in slice 2, combat state in slice 3; identity
// (type/team/health/state/selection) stays on Unit permanently.
struct Orders
{
    Stance stance = Stance::Guard;
    bool autoRetreat = false; // player opt-in auto-retreat
    bool attackMove = false;
    Vector2 attackMoveDest = {}; // march goal kept across chase detours
    bool hasPatrol = false;
    Vector2 patrolA = {};
    Vector2 patrolB = {};
    bool patrolToB = true;
    bool hasRepairOrder = false; // channeled Engineer repair
    Entity repairTarget = kInvalidEntity;
    bool hasAttackGroundOrder = false;
    Vector2 attackGroundPos = {};
    bool hasLoadOrder = false; // G4 player-only transport boarding
    Entity loadTarget = kInvalidEntity; // foot passenger to pick up
    bool hasUnloadOrder = false;
    Vector2 unloadPos = {}; // tile-snapped drop point
    std::vector<QueuedOrder> orderQueue; // pending orders, dispatched FIFO
};

inline Orders &GetOrders(Registry &registry, Entity entity)
{
    if (Orders *orders = registry.Get<Orders>(entity))
    {
        return *orders;
    }
    registry.Add(entity, Orders{});
    return *registry.Get<Orders>(entity);
}

inline const Orders *FindOrders(const Registry &registry, Entity entity)
{
    return registry.Get<Orders>(entity);
}

// --- Core behavior components (Unit split, slice 2: movement) ---
// Movement execution lives here, not on Unit: pending destination, A*
// waypoints, and blocked-retry accounting. Attached at spawn for every
// unit; GetMover creates a default on first use so callers never branch
// on missing. Combat state moved in slice 3; stats stay on Unit.
struct Mover
{
    Vector2 moveTarget = {}; // tile-snapped pending move destination
    bool hasMoveOrder = false;
    std::vector<cc::IVec2> path; // A* waypoints (tile indices)
    std::size_t pathNext = 0;
    bool hasPath = false;
    float blockedTime = 0.0f; // blocked-move retry accounting
    int blockedRepaths = 0;
    float speedCapPixelsPerSec = -1.0f; // slowest-speed cap; -1 = uncapped
};

inline Mover &GetMover(Registry &registry, Entity entity)
{
    if (Mover *mover = registry.Get<Mover>(entity))
    {
        return *mover;
    }
    registry.Add(entity, Mover{});
    return *registry.Get<Mover>(entity);
}

inline const Mover *FindMover(const Registry &registry, Entity entity)
{
    return registry.Get<Mover>(entity);
}

// Effective movement speed honoring the slowest-speed cap (-1 = own speed).
inline float EffectiveSpeed(const Unit &unit, const Mover &mover)
{
    if (mover.speedCapPixelsPerSec < 0.0f || mover.speedCapPixelsPerSec >= unit.speed)
    {
        return unit.speed;
    }
    return mover.speedCapPixelsPerSec;
}

// --- Core behavior components (Unit split, slice 3: combat) ---
// Live combat state lives here, not on Unit: acquired target, attack-phase
// machine, cooldown countdown, and hit feedback. Attached at spawn for
// every unit; GetCombatState creates a default on first use so callers
// never branch on missing. Stats (attackPower/attackRange/cooldownTime/
// windupTime/damageType/speed/sightRange) stay on Unit by decision: they
// are write-once, read-many with no aliasing benefit from pooling.
struct CombatState
{
    Entity target = kInvalidEntity; // acquired enemy
    AttackPhase phase = AttackPhase::Ready;
    float phaseTime = 0.0f; // live WindUp countdown
    float cooldown = 0.0f; // seconds until next strike
    float lastDamageTaken = 0.0f; // most recent effective hit
    float hitFlashTime = 0.0f; // live hit-overlay countdown
};

inline CombatState &GetCombatState(Registry &registry, Entity entity)
{
    if (CombatState *combat = registry.Get<CombatState>(entity))
    {
        return *combat;
    }
    registry.Add(entity, CombatState{});
    return *registry.Get<CombatState>(entity);
}

inline const CombatState *FindCombatState(const Registry &registry, Entity entity)
{
    return registry.Get<CombatState>(entity);
}
