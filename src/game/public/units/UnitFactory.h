#pragma once

#include "Event.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Subsystem.h"
#include "Unit.h"
#include "raylib.h"

// Unit production cost (placeholder balance).
struct UnitCost
{
    long iron = 0;
    long oil = 0;
};

UnitCost CostOf(UnitType type);

// Lifecycle event payload (position + team) that bare Event doesn't carry.
struct UnitLifecycleEvent : Event
{
    Vector2 position = {};
    int teamID = 0;
};

// Spawns units with base stats applied, deducting costs and emitting
// UnitSpawned/UnitDestroyed events.
class UnitFactory : public Subsystem
{
public:
    UnitFactory(Registry &registry, ResourceSystem &resources, EventDispatcher &events);

    // Validate funds via TrySpend, then create + stat + snap + announce.
    Entity Spawn(UnitType type, int teamID, Vector2 worldPos);
    // Create + stat + snap + announce without charging (prepaid via queue).
    Entity SpawnPrepaid(UnitType type, int teamID, Vector2 worldPos);
    // Announce destruction, then remove.
    void DestroyUnit(Entity entity);

private:
    Registry &registry_;
    ResourceSystem &resources_;
    EventDispatcher &events_;
};
