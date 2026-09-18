#pragma once

#include "Event.h"         // EventDispatcher, UnitSpawned/UnitDestroyed routing
#include "Registry.h"      // Entity, Registry
#include "ResourceSystem.h" // cost validation ( economy hooks here later)
#include "Unit.h"          // UnitType
#include "raylib.h"        // Vector2

// Unit production costs (placeholder balance; retunes combat
// values, owns income/gathering — the factory only spends).
struct UnitCost
{
    long iron = 0;
    long oil = 0;
};

UnitCost CostOf(UnitType type);

// Spawns units with base stats applied, deducting costs and emitting
// lifecycle events (UnitSpawned/UnitDestroyed) for UI and tests.
// QoL pings need position + team, which bare Event doesn't carry —
// lifecycle events dispatch this derived payload instead. Subscribers that
// don't care keep taking `const Event&` and ignore the rest.
struct UnitLifecycleEvent : Event
{
    Vector2 position = {};
    int teamID = 0;
};

class UnitFactory{
public:
    UnitFactory(Registry &registry, ResourceSystem &resources, EventDispatcher &events);

    // Validate funds via TrySpend, then create + stat + snap + announce.
    // Returns kInvalidEntity (spending nothing) when funds are short.
    Entity Spawn(UnitType type, int teamID, Vector2 worldPos);
    // Create + stat + snap + announce WITHOUT charging (for the production
    // queue, which collects payment at Enqueue time). Never fails.
    Entity SpawnPrepaid(UnitType type, int teamID, Vector2 worldPos);
    // Announce destruction, then remove. No-op (no event) for dead/missing IDs.
    void DestroyUnit(Entity entity);

private:
    Registry &registry_;
    ResourceSystem &resources_;
    EventDispatcher &events_;
};
