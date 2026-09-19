#include "Production.h"

#include "ResourceSystem.h"
#include "UnitFactory.h"

float BuildTime(UnitType type)
{
    const UnitCost cost = CostOf(type);
    const float timed = 2.0f + static_cast<float>(cost.iron) / 50.0f;
    return timed < 2.0f ? 2.0f : timed;
}

bool ProductionQueue::Enqueue(ResourceSystem &resources, UnitType type, bool repeat)
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
    item.repeat = repeat;
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

void ProductionQueue::SetRepeatArmed(UnitType type, bool repeat)
{
    const int i = static_cast<int>(type);
    if (i >= 0 && i < kTypeCount)
    {
        repeatArmed_[i] = repeat;
    }
}

bool ProductionQueue::RepeatArmed(UnitType type) const
{
    const int i = static_cast<int>(type);
    return i >= 0 && i < kTypeCount && repeatArmed_[i];
}

Entity ProductionQueue::Update(UnitFactory &factory, ResourceSystem &resources, int teamID,
                               Vector2 rallyPos, float dt)
{
    if (items_.empty() || dt <= 0.0f)
    {
        return kInvalidEntity;
    }
    Item &head = items_.front();
    head.progress += dt;
    if (head.progress < head.buildTime)
    {
        return kInvalidEntity;
    }
    if (head.repeat)
    {
        const UnitCost cost = CostOf(head.type);
        if (!resources.TrySpend(cost.iron, cost.oil))
        {
            head.progress = head.buildTime;
            return kInvalidEntity;
        }
        const Entity spawned = factory.SpawnPrepaid(head.type, teamID, rallyPos);
        head.progress = 0.0f;
        return spawned;
    }
    const Item done = head;
    const Entity spawned = factory.SpawnPrepaid(done.type, teamID, rallyPos);
    const float overflow = done.progress - done.buildTime;
    items_.erase(items_.begin());
    if (!items_.empty() && overflow > 0.0f)
    {
        items_.front().progress += overflow;
    }
    return spawned;
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
