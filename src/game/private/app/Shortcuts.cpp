#include "Shortcuts.h"

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
    it->second.plain();
    return true;
}

bool ShortcutRegistry::FireChord(int raylibKey) const
{
    auto it = bindings_.find(raylibKey);
    if (it == bindings_.end() || !it->second.hasChord)
    {
        return false;
    }
    it->second.chord();
    return true;
}

void ShortcutRegistry::PollAndFire() const
{
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (const auto &pair : bindings_)
    {
        if (!IsKeyPressed(pair.first))
        {
            continue;
        }
        // Chord takes priority when Shift is held so a key bound both ways
        // (F6 save / Shift+F6 load) fires exactly one action. A plain
        // binding with no chord still fires under Shift (Shift-extended
        // box-select must not swallow Space/Esc/P/A/etc).
        if (shift && pair.second.hasChord)
        {
            pair.second.chord();
        }
        else if (pair.second.hasPlain)
        {
            pair.second.plain();
        }
    }
}

void ShortcutRegistry::Clear()
{
    bindings_.clear();
}
