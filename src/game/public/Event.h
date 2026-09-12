#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

// M1 Goal 5: event dispatcher architecture. Gameplay systems (M3/M4/M5) post
// events through EventDispatcher; UI (M6) and tests (M7) subscribe to them.
// Listener routing is by EventType; handlers receive the base Event and
// downcast to the concrete payload type for their event.

enum class EventType
{
    None,
    UnitSpawned,
    UnitDestroyed,
    ResourceChanged
};

struct Event
{
    EventType type = EventType::None;
    virtual ~Event() = default;
};

class EventDispatcher
{
public:
    using Handler = std::function<void(const Event &)>;

    // Register a handler invoked for every Dispatch of the given type.
    void Subscribe(EventType type, Handler handler);
    // Invoke all handlers registered for event.type, in subscription order.
    void Dispatch(const Event &event) const;
    // Drop all subscriptions (teardown / test isolation).
    void Clear();

private:
    std::unordered_map<EventType, std::vector<Handler>> listeners_;
};
