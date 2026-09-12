#pragma once

#include "Event.h"         // EventDispatcher, UnitSpawned/UnitDestroyed routing
#include "Registry.h"      // Entity, Registry
#include "ResourceSystem.h" // cost validation (M5 economy hooks here later)
#include "Unit.h"          // UnitType
#include "raylib.h"        // Vector2

// M3 Goal 6: unit production costs (placeholder balance; M4 retunes combat
// values, M5 owns income/gathering — the factory only spends).
struct UnitCost
{
    long iron = 0;
    long oil = 0;
};

UnitCost CostOf(UnitType type);

// Spawns units with base stats applied, deducting costs and emitting
// lifecycle events (UnitSpawned/UnitDestroyed) for UI (M6) and tests.
class UnitFactory
{
public:
    UnitFactory(Registry &registry, ResourceSystem &resources, EventDispatcher &events);

    // Validate funds via TrySpend, then create + stat + snap + announce.
    // Returns kInvalidEntity (spending nothing) when funds are short.
    Entity Spawn(UnitType type, int teamID, Vector2 worldPos);
    // Announce destruction, then remove. No-op (no event) for dead/missing IDs.
    void DestroyUnit(Entity entity);

private:
    Registry &registry_;
    ResourceSystem &resources_;
    EventDispatcher &events_;
};
