#include "Registry.h"

// M1 Goal 6: non-template entity lifecycle. Component pools live in the header.

Entity Registry::Create()
{
    Entity entity;
    if (!free_.empty())
    {
        entity = free_.back();
        free_.pop_back();
    }
    else
    {
        entity = next_++;
    }
    alive_.insert(entity);
    return entity;
}

void Registry::Destroy(Entity entity)
{
    if (alive_.erase(entity) == 0)
    {
        return; // destroying a dead/unknown entity is a no-op
    }
    for (auto &[type, pool] : pools_)
    {
        pool->RemoveEntity(entity);
    }
    free_.push_back(entity);
}

bool Registry::IsAlive(Entity entity) const
{
    return alive_.count(entity) > 0;
}

std::size_t Registry::EntityCount() const
{
    return alive_.size();
}

void Registry::Clear()
{
    for (auto &[type, pool] : pools_)
    {
        pool->ClearAll();
    }
    alive_.clear();
    free_.clear();
    next_ = 1;
}
