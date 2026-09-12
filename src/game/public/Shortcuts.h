#pragma once

#include <functional>
#include <unordered_map>

// M2 Goal 5: keyboard shortcut registry. Keys are raylib key codes
// (KEY_ESCAPE, KEY_SPACE, ...); actions are nullary callbacks bound by
// game code. PollAndFire polls live input (edge-triggered IsKeyPressed);
// Fire invokes one binding directly so tests cover routing headless.

class ShortcutRegistry
{
public:
    using Action = std::function<void()>;

    // Bind (or rebind) a key to an action.
    void Bind(int raylibKey, Action action);
    void Unbind(int raylibKey);
    bool Has(int raylibKey) const;

    // Invoke the action bound to raylibKey; false when unbound.
    bool Fire(int raylibKey) const;

    // Fire each binding whose key was pressed this frame.
    void PollAndFire() const;

    void Clear();

private:
    std::unordered_map<int, Action> bindings_;
};
