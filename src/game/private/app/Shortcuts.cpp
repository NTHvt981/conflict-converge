#include "Shortcuts.h"

#include <vector>

#include "raylib.h"

void ShortcutRegistry::Bind(int raylibKey, Action action)
{
    KeyBindings &slot = bindings_[raylibKey];
    slot.plain = std::move(action);
    slot.hasPlain = true;
}

void ShortcutRegistry::BindChord(int raylibKey, Action action)
{
    KeyBindings &slot = bindings_[raylibKey];
    slot.chord = std::move(action);
    slot.hasChord = true;
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
    if (!enabled_)
    {
        return false;
    }
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end() || !it->second.hasPlain)
    {
        return false;
    }
    const Action action = it->second.plain;
    action();
    return true;
}

bool ShortcutRegistry::FireChord(int raylibKey) const
{
    if (!enabled_)
    {
        return false;
    }
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end() || !it->second.hasChord)
    {
        return false;
    }
    const Action action = it->second.chord;
    action();
    return true;
}

bool ShortcutRegistry::FireWithShift(int raylibKey, bool shift) const
{
    if (!enabled_)
    {
        return false;
    }
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end())
    {
        return false;
    }
    Action action;
    if (shift && it->second.hasChord)
    {
        action = it->second.chord;
    }
    else if (it->second.hasPlain)
    {
        action = it->second.plain;
    }
    else
    {
        return false;
    }
    action();
    return true;
}

void ShortcutRegistry::PollAndFire() const
{
    if (!enabled_)
    {
        return;
    }
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    std::vector<int> keys;
    keys.reserve(bindings_.size());
    for (const auto &pair : bindings_)
    {
        keys.push_back(pair.first);
    }
    for (int key : keys)
    {
        if (IsKeyPressed(key))
        {
            FireWithShift(key, shift);
        }
    }
}

void ShortcutRegistry::Clear()
{
    bindings_.clear();
}
