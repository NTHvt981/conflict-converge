#pragma once

#include "core/Event.h"
#include "core/Registry.h"
#include "economy/ResourceSystem.h"
#include "core/Subsystem.h"
#include "core/Team.h"
#include "units/Unit.h"
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
	TeamId teamID = kPlayerTeam;
};

class UnitFactory : public Subsystem
{
public:
	UnitFactory(Registry &registry, ResourceSystem &resources, EventDispatcher &events);

	Entity Spawn(UnitType type, TeamId teamID, Vector2 worldPos);
	// Without charging (prepaid via queue).
	Entity SpawnPrepaid(UnitType type, TeamId teamID, Vector2 worldPos);
	void DestroyUnit(Entity entity);

private:
	Registry &registry_;
	ResourceSystem &resources_;
	EventDispatcher &events_;
};
