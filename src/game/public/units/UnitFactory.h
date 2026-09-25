#pragma once

#include "Event.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Subsystem.h"
#include "Unit.h"
#include "raylib.h"

struct UnitCost
{
    long iron = 0;
    long oil = 0;
};

UnitCost CostOf(UnitType type);

struct UnitLifecycleEvent : Event
{
    Vector2 position = {};
    int teamID = 0;
};

class UnitFactory : public Subsystem
{
public:
    UnitFactory(Registry &registry, ResourceSystem &resources, EventDispatcher &events);

    Entity Spawn(UnitType type, int teamID, Vector2 worldPos);
    // Without charging (prepaid via queue).
    Entity SpawnPrepaid(UnitType type, int teamID, Vector2 worldPos);
    void DestroyUnit(Entity entity);

private:
    Registry &registry_;
    ResourceSystem &resources_;
    EventDispatcher &events_;
};
