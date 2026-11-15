// Unit tests for QoL control groups (assign/add/recall via bitmask).

#include "test_harness.h"

#include "core/Event.h"
#include "economy/Production.h"
#include "economy/ResourceSystem.h"
#include "app/input/Selection.h"
#include "units/Unit.h"
#include "units/UnitFactory.h"

namespace
{

Entity SpawnAt(Registry &registry, float x, float y)
{
	Entity entity = registry.Create();
	Unit unit;
	unit.position = { x, y };
	SnapUnitToTile(unit);
	registry.Add(entity, unit);
	return entity;
}

void Select(Registry &registry, Entity id)
{
	DeselectAll(registry);
	if (Unit *unit = registry.Get<Unit>(id))
	{
		unit->isSelected = true;
	}
}

} // namespace

void RunControlGroupTests()
{
	Registry registry;
	const Entity a = SpawnAt(registry, 64.0f, 64.0f);
	const Entity b = SpawnAt(registry, 256.0f, 128.0f);

	Select(registry, a);
	CC_CHECK(AssignControlGroup(registry, 0) == 1);
	CC_CHECK((registry.Get<Unit>(a)->controlGroups & 1u) != 0);
	CC_CHECK((registry.Get<Unit>(b)->controlGroups & 1u) == 0);

	Select(registry, b);
	CC_CHECK(AddToControlGroup(registry, 0) == 1);
	CC_CHECK((registry.Get<Unit>(a)->controlGroups & 1u) != 0);
	CC_CHECK((registry.Get<Unit>(b)->controlGroups & 1u) != 0);

	DeselectAll(registry);
	CC_CHECK(RecallControlGroup(registry, 0) == 2);
	CC_CHECK(registry.Get<Unit>(a)->isSelected && registry.Get<Unit>(b)->isSelected);

	Select(registry, a);
	CC_CHECK(AssignControlGroup(registry, 0) == 1);
	CC_CHECK(RecallControlGroup(registry, 0) == 1);
	CC_CHECK(registry.Get<Unit>(a)->isSelected);
	CC_CHECK(!registry.Get<Unit>(b)->isSelected);

	Select(registry, b);
	CC_CHECK(AssignControlGroup(registry, 3) == 1);
	CC_CHECK((registry.Get<Unit>(b)->controlGroups & (1u << 3)) != 0);
	CC_CHECK((registry.Get<Unit>(a)->controlGroups & (1u << 3)) == 0);
	CC_CHECK(RecallControlGroup(registry, 0) == 1); // group 0 undisturbed

	CC_CHECK(AssignControlGroup(registry, -1) == 0);
	CC_CHECK(AssignControlGroup(registry, 10) == 0);
	CC_CHECK(AddToControlGroup(registry, 99) == 0);
	CC_CHECK(RecallControlGroup(registry, -1) == 0);

	DeselectAll(registry);
	CC_CHECK(RecallControlGroup(registry, 5) == 0);
	CC_CHECK(SelectedUnit(registry) == kInvalidEntity);

	Registry prodRegistry;
	ResourceSystem resources;
	resources.AddIron(1000);
	resources.AddOil(500);
	EventDispatcher events;
	UnitFactory factory(prodRegistry, resources, events);
	ProductionQueue queue;
	CC_CHECK(queue.Enqueue(resources, UnitType::RifleInfantry));
	const Entity spawned =
		queue.Update(factory, resources, kPlayerTeam, { 0.0f, 0.0f },
					 BuildTime(UnitType::RifleInfantry));
	CC_CHECK(spawned != kInvalidEntity);
	CC_CHECK(prodRegistry.Get<Unit>(spawned) != nullptr);
	CC_CHECK(queue.Update(factory, resources, kPlayerTeam, { 0.0f, 0.0f },
						  1000.0f) == kInvalidEntity);
}
