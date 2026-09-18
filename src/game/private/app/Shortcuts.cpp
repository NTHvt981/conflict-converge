#include "Shortcuts.h"

#include <vector> // PollAndFire key snapshot

#include "raylib.h" // IsKeyPressed (live polling only in PollAndFire)

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
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end() || !it->second.hasPlain)
    {
        return false;
    }
    // Copy before invoking: the action may re-entrantly Bind/Unbind/Clear,
    // destroying the map node (and its std::function) while it runs.
    const Action action = it->second.plain;
    action();
    return true;
}

bool ShortcutRegistry::FireChord(int raylibKey) const
{
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end() || !it->second.hasChord)
    {
        return false;
    }
    // Copy before invoking: see Fire.
    const Action action = it->second.chord;
    action();
    return true;
}

bool ShortcutRegistry::FireWithShift(int raylibKey, bool shift) const
{
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end())
    {
        return false;
    }
    // Copy before invoking (see Fire): the action may destroy its own
    // binding re-entrantly.
    Action action;
    // Chord takes priority when Shift is held so a key bound both ways
    // (F6 save / Shift+F6 load) fires exactly one action. A plain
    // binding with no chord still fires under Shift (Shift-extended
    // box-select must not swallow Space/Esc/P/A/etc).
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
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    // Snapshot keys: a fired action may re-entrantly Bind/Unbind/Clear,
    // rehashing the map mid-iteration. Pressed-state and firing go through
    // the live table per key, so same-poll mutations of not-yet-fired keys
    // apply; newly added keys wait for the next frame.
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
