
#include "units/Unit.h"

#include "units/UnitInternal.h"
#include "units/Extensions.h"
#include "world/MapFile.h"

bool CanLoadTarget(const Registry &registry, Entity carrier, const Unit &carrierUnit,
                   Entity passenger)
{
    if (passenger == carrier || passenger == kInvalidEntity)
    {
        return false;
    }
    const Cargo *cargo = registry.Get<Cargo>(carrier);
    if (cargo == nullptr || cargo->capacity <= 0 ||
        cargo->passengers.size() >= static_cast<std::size_t>(cargo->capacity))
    {
        return false;
    }
    const Unit *rider = registry.Get<Unit>(passenger);
    if (rider == nullptr || rider->health <= 0.0f || rider->teamID != carrierUnit.teamID)
    {
        return false;
    }
    if (!IsFleshUnit(rider->type) || IsEmbarked(registry, passenger))
    {
        return false;
    }
    return true;
}

bool BoardTransport(Registry &registry, Entity carrier, Entity passenger)
{
    Unit *carrierUnit = registry.Get<Unit>(carrier);
    Unit *rider = registry.Get<Unit>(passenger);
    if (carrierUnit == nullptr || rider == nullptr ||
        !CanLoadTarget(registry, carrier, *carrierUnit, passenger))
    {
        return false;
    }
    EmbarkedOn ride;
    ride.carrier = carrier;
    registry.Add(passenger, ride);
    registry.Get<Cargo>(carrier)->passengers.push_back(passenger);
    rider->isSelected = false;
    Mover &riderMover = GetMover(registry, passenger);
    riderMover.hasMoveOrder = false;
    riderMover.hasPath = false;
    riderMover.path.clear();
    riderMover.pathNext = 0;
    rider->velocity = { 0.0f, 0.0f };
    rider->state = UnitState::Idle;
    return true;
}

int UnloadTransport(Registry &registry, const TileMap &map, Entity carrier, Vector2 worldPos)
{
    Cargo *cargo = registry.Get<Cargo>(carrier);
    Unit *carrierUnit = registry.Get<Unit>(carrier);
    if (cargo == nullptr || carrierUnit == nullptr || cargo->passengers.empty())
    {
        return 0;
    }
    const cc::IVec2 base = cc::WorldToTile(cc::ToGlm(worldPos));
    int landed = 0;
    for (const Entity passenger : cargo->passengers)
    {
        Unit *rider = registry.Get<Unit>(passenger);
        if (rider == nullptr)
        {
            continue;
        }
        const cc::IVec2 dest = NearestFreeTile(map, base.x, base.y);
        rider->position = cc::ToRaylib(cc::TileToWorld(dest.x, dest.y));
        SnapUnitToTile(*rider);
        registry.Remove<EmbarkedOn>(passenger);
        ++landed;
    }
    cargo->passengers.clear();
    return landed;
}

void IssueLoadOrder(Unit &carrier, Orders &orders, Mover &mover, Entity passenger)
{
    StopMoving(carrier, mover);
    ClearOrders(carrier, orders, mover);
    orders.hasLoadOrder = true;
    orders.loadTarget = passenger;
}

void IssueUnloadOrder(Unit &carrier, Orders &orders, Mover &mover, const TileMap &map,
                      Vector2 worldPos)
{
    (void)map;
    StopMoving(carrier, mover);
    ClearOrders(carrier, orders, mover);
    orders.hasUnloadOrder = true;
    orders.unloadPos = cc::ToRaylib(cc::SnapToTile(cc::ToGlm(worldPos)));
}
