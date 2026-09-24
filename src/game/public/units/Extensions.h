#pragma once

#include <vector>

#include "Combat.h"
#include "Registry.h"

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
