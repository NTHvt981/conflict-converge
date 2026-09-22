#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "Subsystem.h"

// Event dispatcher: systems post events, UI and tests subscribe. Routing is
// by EventType; handlers downcast the base Event to the concrete payload.

enum class EventType
{
    None,
    UnitSpawned,
    UnitDestroyed,
    ResourceChanged,
    UnitDamaged,
    ResourceDepleted,
    MatchStarted,
    MatchPaused,
    GameOver,
    Victory,
    MenuAction,
    ProductionOrdered
};

struct Event
{
    EventType type = EventType::None;
    virtual ~Event() = default;
};

class EventDispatcher : public Subsystem
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
