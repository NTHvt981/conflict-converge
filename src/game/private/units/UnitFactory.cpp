#include "UnitFactory.h"

#include "MathUtils.h" // SnapToTile
#include "UnitStats.h" // ApplyBaseStats

UnitCost CostOf(UnitType type)
{
    // Placeholder price list: infantry is cheap iron, vehicles mix in oil,
    // heavies cost both. M4/M5 retune with real balance + income numbers.
    switch (type)
    {
    case UnitType::Infantry:
        return { 25, 0 };
    case UnitType::AntiArmorInfantry:
        return { 30, 5 };
    case UnitType::Engineer:
        return { 40, 0 };
    case UnitType::IFV:
        return { 80, 20 };
    case UnitType::Artillery:
        return { 120, 40 };
    case UnitType::LightTank:
        return { 150, 50 };
    case UnitType::HeavyTank:
        return { 250, 100 };
    }
    return { 0, 0 };
}

UnitFactory::UnitFactory(Registry &registry, ResourceSystem &resources, EventDispatcher &events)
    : registry_(registry), resources_(resources), events_(events)
{
}

Entity UnitFactory::Spawn(UnitType type, int teamID, Vector2 worldPos)
{
    const UnitCost cost = CostOf(type);
    if (!resources_.TrySpend(cost.iron, cost.oil))
    {
        return kInvalidEntity;
    }
    return SpawnPrepaid(type, teamID, worldPos);
}

Entity UnitFactory::SpawnPrepaid(UnitType type, int teamID, Vector2 worldPos)
{
    Unit unit;
    unit.type = type;
    ApplyBaseStats(unit);
    unit.teamID = teamID;
    unit.position = worldPos;
    SnapUnitToTile(unit);

    const Entity id = registry_.Create();
    registry_.Add(id, unit);

    Event spawned;
    spawned.type = EventType::UnitSpawned;
    events_.Dispatch(spawned);
    return id;
}

void UnitFactory::DestroyUnit(Entity entity)
{
    if (!registry_.IsAlive(entity))
    {
        return;
    }
    Event destroyed;
    destroyed.type = EventType::UnitDestroyed;
    events_.Dispatch(destroyed);
    registry_.Destroy(entity);
}
