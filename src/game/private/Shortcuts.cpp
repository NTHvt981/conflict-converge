#include "Shortcuts.h"

#include "raylib.h" // IsKeyPressed (live polling only in PollAndFire)

void ShortcutRegistry::Bind(int raylibKey, Action action)
{
    bindings_[raylibKey] = Binding{ std::move(action), false };
}

void ShortcutRegistry::BindChord(int raylibKey, Action action)
{
    bindings_[raylibKey] = Binding{ std::move(action), true };
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
    if (it == bindings_.end() || it->second.requireShift)
    {
        return false;
    }
    it->second.action();
    return true;
}

bool ShortcutRegistry::FireChord(int raylibKey) const
{
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end() || !it->second.requireShift)
    {
        return false;
    }
    it->second.action();
    return true;
}

void ShortcutRegistry::PollAndFire() const
{
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (const auto &pair : bindings_)
    {
        if (IsKeyPressed(pair.first) && pair.second.requireShift == shift)
        {
            pair.second.action();
        }
    }
}

void ShortcutRegistry::Clear()
{
    bindings_.clear();
}
