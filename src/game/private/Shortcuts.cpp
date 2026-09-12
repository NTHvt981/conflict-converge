#include "Shortcuts.h"

#include "raylib.h" // IsKeyPressed (live polling only in PollAndFire)

void ShortcutRegistry::Bind(int raylibKey, Action action)
{
    bindings_[raylibKey] = std::move(action);
}

void ShortcutRegistry::Unbind(int raylibKey)
{
    bindings_.erase(raylibKey);
}

bool ShortcutRegistry::Has(int raylibKey) const
{
    return bindings_.count(raylibKey) > 0;
}

bool ShortcutRegistry::Fire(int raylibKey) const
{
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end())
    {
        return false;
    }
    it->second();
    return true;
}

void ShortcutRegistry::PollAndFire() const
{
    for (const auto &pair : bindings_)
    {
        if (IsKeyPressed(pair.first))
        {
            pair.second();
        }
    }
}

void ShortcutRegistry::Clear()
{
    bindings_.clear();
}
