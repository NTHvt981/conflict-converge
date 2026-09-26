#include "units/UnitFactory.h"

#include "units/Extensions.h"
#include "core/MathUtils.h"
#include "app/data/UnitConfig.h"
#include "units/UnitStats.h"

UnitCost CostOf(UnitType type)
{
    const UnitConfig &config = ActiveUnitConfig(type);
    return { config.costIron, config.costOil };
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
    registry_.Add(id, Orders{});
    registry_.Add(id, Mover{});
    registry_.Add(id, CombatState{});
    const UnitConfig &config = ActiveUnitConfig(type);
    if (config.abilities.turretTurnRate > 0.0f)
    {
        Turret turret;
        turret.turnRate = config.abilities.turretTurnRate;
        registry_.Add(id, turret);
    }
    if (config.abilities.transportCapacity > 0)
    {
        Cargo cargo;
        cargo.capacity = config.abilities.transportCapacity;
        registry_.Add(id, cargo);
    }

    UnitLifecycleEvent spawned;
    spawned.type = EventType::UnitSpawned;
    spawned.position = worldPos;
    spawned.teamID = teamID;
    events_.Dispatch(spawned);
    return id;
}

void UnitFactory::DestroyUnit(Entity entity)
{
    if (!registry_.IsAlive(entity))
    {
        return;
    }
    if (Cargo *cargo = registry_.Get<Cargo>(entity))
    {
        if (const Unit *carrier = registry_.Get<Unit>(entity))
        {
            for (const Entity passenger : cargo->passengers)
            {
                if (Unit *u = registry_.Get<Unit>(passenger))
                {
                    u->position = carrier->position;
                    SnapUnitToTile(*u);
                }
                registry_.Remove<EmbarkedOn>(passenger);
            }
        }
        cargo->passengers.clear();
    }
    UnitLifecycleEvent destroyed;
    destroyed.type = EventType::UnitDestroyed;
    if (const Unit *unit = registry_.Get<Unit>(entity))
    {
        destroyed.position = unit->position;
        destroyed.teamID = unit->teamID;
    }
    events_.Dispatch(destroyed);
    registry_.Destroy(entity);
}
