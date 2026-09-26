#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/CcAssert.h"

// Auto-instanced service base (UE5-inspired, no reflection). Subsystems are
// owned by a Subsystems container; lifetime is explicit Add + InitAll/Shutdown.
// Engine scope lives for the process, world scope resets per match, player
// scope holds per-viewport state. No global accessor: pass Subsystems & explicitly.

class Subsystem
{
public:
    virtual ~Subsystem() = default;
    virtual void Init()
    {
    }
    virtual void Shutdown()
    {
    }
    virtual void ResetForMatch()
    {
    }
};

class Subsystems
{
public:
    Subsystems() = default;
    Subsystems(const Subsystems &) = delete;
    Subsystems &operator=(const Subsystems &) = delete;
    Subsystems(Subsystems &&) = delete;
    Subsystems &operator=(Subsystems &&) = delete;

    template <class T, class... A> T &Add(A &&...a);
    template <class T, class... A> T &AddKeyed(const std::string &key, A &&...a);

    template <class T> T &Get();
    template <class T> T *TryGet() noexcept;
    template <class T> T &GetKeyed(const std::string &key);
    template <class T> bool Has() const;

    void InitAll();
    void ResetForMatch();
    void Shutdown();
    std::size_t Size() const;

private:
    struct Key
    {
        std::type_index type;
        std::string name;

        Key(std::type_index type, std::string name)
            : type(type)
            , name(std::move(name))
        {
        }

        bool operator==(const Key &other) const
        {
            return type == other.type && name == other.name;
        }
    };

    struct KeyHash
    {
        std::size_t operator()(const Key &key) const noexcept
        {
            const std::size_t h1 = std::hash<std::type_index>{}(key.type);
            const std::size_t h2 = std::hash<std::string>{}(key.name);
            return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
        }
    };

    Subsystem &Insert(std::type_index type, const std::string &name,
        std::unique_ptr<Subsystem> owned);
    Subsystem *Find(std::type_index type, const std::string &name) const;

    std::vector<std::unique_ptr<Subsystem>> order_;
    std::unordered_map<Key, Subsystem *, KeyHash> index_;
};

inline Subsystem &Subsystems::Insert(std::type_index type, const std::string &name,
    std::unique_ptr<Subsystem> owned)
{
    CC_ASSERT(owned != nullptr);
    CC_ASSERT(Find(type, name) == nullptr);
    order_.push_back(std::move(owned));
    Subsystem *raw = order_.back().get();
    index_.emplace(Key(type, name), raw);
    return *raw;
}

inline Subsystem *Subsystems::Find(std::type_index type, const std::string &name) const
{
    const auto it = index_.find(Key(type, name));
    return it == index_.end() ? nullptr : it->second;
}

template <class T, class... A> T &Subsystems::Add(A &&...a)
{
    static_assert(std::is_base_of_v<Subsystem, T>);
    return static_cast<T &>(Insert(std::type_index(typeid(T)), std::string(),
        std::make_unique<T>(std::forward<A>(a)...)));
}

template <class T, class... A> T &Subsystems::AddKeyed(const std::string &key, A &&...a)
{
    static_assert(std::is_base_of_v<Subsystem, T>);
    return static_cast<T &>(Insert(std::type_index(typeid(T)), key,
        std::make_unique<T>(std::forward<A>(a)...)));
}

template <class T> T &Subsystems::Get()
{
    Subsystem *found = Find(std::type_index(typeid(T)), std::string());
    CC_ASSERT(found != nullptr);
    return *static_cast<T *>(found);
}

template <class T> T *Subsystems::TryGet() noexcept
{
    return static_cast<T *>(Find(std::type_index(typeid(T)), std::string()));
}

template <class T> T &Subsystems::GetKeyed(const std::string &key)
{
    Subsystem *found = Find(std::type_index(typeid(T)), key);
    CC_ASSERT(found != nullptr);
    return *static_cast<T *>(found);
}

template <class T> bool Subsystems::Has() const
{
    return Find(std::type_index(typeid(T)), std::string()) != nullptr;
}

inline void Subsystems::InitAll()
{
    for (const auto &owned : order_)
    {
        owned->Init();
    }
}

inline void Subsystems::ResetForMatch()
{
    for (const auto &owned : order_)
    {
        owned->ResetForMatch();
    }
}

inline void Subsystems::Shutdown()
{
    for (auto it = order_.rbegin(); it != order_.rend(); ++it)
    {
        (*it)->Shutdown();
    }
    order_.clear();
    index_.clear();
}

inline std::size_t Subsystems::Size() const
{
    return order_.size();
}
