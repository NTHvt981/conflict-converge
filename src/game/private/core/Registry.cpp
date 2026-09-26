#include "core/Registry.h"

Entity Registry::Create()
{
    Entity entity;
    if (!free_.empty())
    {
        entity = free_.back();
        free_.pop_back();
        generations_[entity] = nextGen_++;
    }
    else
    {
        entity = next_++;
        generations_[entity] = nextGen_++;
    }
    alive_.insert(entity);
    return entity;
}

void Registry::Destroy(Entity entity)
{
    if (alive_.erase(entity) == 0)
    {
        return;
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

std::uint32_t Registry::Generation(Entity entity) const
{
    auto it = generations_.find(entity);
    return it == generations_.end() ? 0 : it->second;
}

void Registry::Clear()
{
    for (auto &[type, pool] : pools_)
    {
        pool->ClearAll();
    }
    alive_.clear();
    free_.clear();
    generations_.clear();
    nextGen_ = 1;
    next_ = 1;
}
