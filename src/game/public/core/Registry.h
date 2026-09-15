#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// M1 Goal 6: ECS-lite registry for component management. Entities are plain
// IDs; components are arbitrary structs stored per type (M2 movement, M3 unit
// stats, M4 combat state all attach here). Systems iterate by querying the
// component pools they care about. No dependencies beyond the STL.

using Entity = std::uint32_t;

// Reserved sentinel: Create() never returns this value.
inline constexpr Entity kInvalidEntity = 0;

class Registry
{
public:
    // --- entity lifecycle ---
    Entity Create();
    void Destroy(Entity entity);
    bool IsAlive(Entity entity) const;
    std::size_t EntityCount() const;

    // Generation counter: increments each time an entity ID is recycled.
    // Occupancy grids store (entity, generation) to detect stale references
    // after destroy+reuse (Phase 4 prerequisite, docs/NOTES.md finding #2).
    std::uint32_t Generation(Entity entity) const;

    // --- component access (per-type pools) ---
    template <typename T> void Add(Entity entity, T component);
    template <typename T> bool Has(Entity entity) const;
    template <typename T> T *Get(Entity entity);
    template <typename T> const T *Get(Entity entity) const;
    template <typename T> void Remove(Entity entity);

    // Visit every component of type T: fn(Entity, T&) (or const T&).
    // M2 Goal 4: selection, orders, and movement update all iterate units.
    template <typename T, typename Fn> void Each(Fn fn);
    template <typename T, typename Fn> void Each(Fn fn) const;

    // Drop all entities and components (teardown / test isolation).
    void Clear();

private:
    struct IPool
    {
        virtual ~IPool() = default;
        virtual void RemoveEntity(Entity entity) = 0;
        virtual void ClearAll() = 0;
    };

    template <typename T> struct Pool : IPool
    {
        std::unordered_map<Entity, T> data;
        void RemoveEntity(Entity entity) override
        {
            data.erase(entity);
        }
        void ClearAll() override
        {
            data.clear();
        }
    };

    template <typename T> std::unordered_map<Entity, T> &MutablePool();
    template <typename T> const std::unordered_map<Entity, T> *FindPool() const;

    std::unordered_map<std::type_index, std::unique_ptr<IPool>> pools_;
    std::unordered_set<Entity> alive_;
    std::vector<Entity> free_;
    std::unordered_map<Entity, std::uint32_t> generations_;
    std::uint32_t nextGen_ = 1;
    Entity next_ = 1;
};

// --- template implementation (header-only; non-template parts in Registry.cpp) ---

template <typename T> std::unordered_map<Entity, T> &Registry::MutablePool()
{
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it == pools_.end())
    {
        auto pool = std::make_unique<Pool<T>>();
        std::unordered_map<Entity, T> &data = pool->data;
        pools_.emplace(std::type_index(typeid(T)), std::move(pool));
        return data;
    }
    return static_cast<Pool<T> *>(it->second.get())->data;
}

template <typename T> const std::unordered_map<Entity, T> *Registry::FindPool() const
{
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it == pools_.end())
    {
        return nullptr;
    }
    return &static_cast<const Pool<T> *>(it->second.get())->data;
}

template <typename T> void Registry::Add(Entity entity, T component)
{
    MutablePool<T>()[entity] = std::move(component);
}

template <typename T> bool Registry::Has(Entity entity) const
{
    const auto *pool = FindPool<T>();
    return pool != nullptr && pool->count(entity) > 0;
}

template <typename T> T *Registry::Get(Entity entity)
{
    auto &pool = MutablePool<T>();
    auto it = pool.find(entity);
    return it == pool.end() ? nullptr : &it->second;
}

template <typename T> const T *Registry::Get(Entity entity) const
{
    const auto *pool = FindPool<T>();
    if (pool == nullptr)
    {
        return nullptr;
    }
    auto it = pool->find(entity);
    return it == pool->end() ? nullptr : &it->second;
}

template <typename T> void Registry::Remove(Entity entity)
{
    // Look up without creating: removing a never-added component is a no-op.
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it != pools_.end())
    {
        static_cast<Pool<T> *>(it->second.get())->data.erase(entity);
    }
}

template <typename T, typename Fn> void Registry::Each(Fn fn)
{
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it == pools_.end())
    {
        return;
    }
    for (auto &pair : static_cast<Pool<T> *>(it->second.get())->data)
    {
        fn(pair.first, pair.second);
    }
}

template <typename T, typename Fn> void Registry::Each(Fn fn) const
{
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it == pools_.end())
    {
        return;
    }
    for (const auto &pair : static_cast<const Pool<T> *>(it->second.get())->data)
    {
        fn(pair.first, pair.second);
    }
}
