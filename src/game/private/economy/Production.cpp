#include "Production.h"

#include "ResourceSystem.h"
#include "UnitFactory.h" // CostOf + SpawnPrepaid

float BuildTime(UnitType type)
{
    const UnitCost cost = CostOf(type);
    const float timed = 2.0f + static_cast<float>(cost.iron) / 50.0f;
    return timed < 2.0f ? 2.0f : timed; // every unit takes at least 2s
}

bool ProductionQueue::Enqueue(ResourceSystem &resources, UnitType type)
{
    const UnitCost cost = CostOf(type);
    if (!resources.TrySpend(cost.iron, cost.oil))
    {
        return false;
    }
    Item item;
    item.type = type;
    item.progress = 0.0f;
    item.buildTime = BuildTime(type);
    items_.push_back(item);
    return true;
}

void ProductionQueue::CancelTop(ResourceSystem &resources)
{
    if (items_.empty())
    {
        return;
    }
    const UnitCost cost = CostOf(items_.front().type);
    resources.AddIron(cost.iron);
    resources.AddOil(cost.oil);
    items_.erase(items_.begin());
}

Entity ProductionQueue::Update(UnitFactory &factory, int teamID, Vector2 rallyPos, float dt)
{
    if (items_.empty() || dt <= 0.0f)
    {
        return kInvalidEntity;
    }
    Item &head = items_.front();
    head.progress += dt;
    // Carry surplus past buildTime into the next item instead of dropping
    // it, so frame hitches and catch-up ticks don't silently slow
    // production. At most one completion per call (the tested "finishes
    // head only" contract); the carried surplus fires the next item sooner.
    if (head.progress >= head.buildTime)
    {
        const Item done = head;
        // Already paid at Enqueue; a short-funded spawn here would eat the
        // item, so SpawnPrepaid (not Spawn) keeps cost handling in one place.
        const Entity spawned = factory.SpawnPrepaid(done.type, teamID, rallyPos);
        const float overflow = done.progress - done.buildTime;
        items_.erase(items_.begin());
        if (!items_.empty() && overflow > 0.0f)
        {
            items_.front().progress += overflow;
        }
        return spawned;
    }
    return kInvalidEntity;
}

bool ProductionQueue::Empty() const
{
    return items_.empty();
}

std::size_t ProductionQueue::Size() const
{
    return items_.size();
}

float ProductionQueue::HeadProgress() const
{
    if (items_.empty())
    {
        return 0.0f;
    }
    const Item &head = items_.front();
    return head.progress / head.buildTime;
}
