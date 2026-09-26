#pragma once

#include <cstddef>
#include <vector>

#include "units/Combat.h"
#include "core/Registry.h"
#include "units/Unit.h"

// Header-only: no TU, no premake regen.

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
    bool autoRepair = false; // Engineer toggle: acquire nearby damage
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
