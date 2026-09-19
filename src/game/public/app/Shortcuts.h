#pragma once

#include <functional>
#include <unordered_map>

// Keyboard shortcut registry. Keys are raylib key codes; actions are nullary
// callbacks bound by game code. PollAndFire polls live input; Fire invokes
// one binding directly so tests cover routing headless.

class ShortcutRegistry
{
public:
    using Action = std::function<void()>;

    // Bind (or rebind) a key to an action.
    void Bind(int raylibKey, Action action);
    // Bind a Shift+key chord; plain Bind never requires shift.
    void BindChord(int raylibKey, Action action);
    void Unbind(int raylibKey);
    bool Has(int raylibKey) const;

    // Invoke the action bound to raylibKey; false when unbound.
    bool Fire(int raylibKey) const;
    // Headless chord path: fires only when a shift-chord is bound.
    bool FireChord(int raylibKey) const;
    // Fire one key honoring PollAndFire's shift-chord priority.
    bool FireWithShift(int raylibKey, bool shift) const;

    // Fire each binding whose key was pressed this frame.
    void PollAndFire() const;

    void Clear();

private:
    struct KeyBindings
    {
        Action plain;
        Action chord;
        bool hasPlain = false;
        bool hasChord = false;
    };
    std::unordered_map<int, KeyBindings> bindings_;
};
