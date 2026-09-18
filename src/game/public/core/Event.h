#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

// Event dispatcher architecture. Gameplay systems post
// events through EventDispatcher; UI and tests subscribe to them.
// Listener routing is by EventType; handlers receive the base Event and
// downcast to the concrete payload type for their event.

enum class EventType
{
    None,
    UnitSpawned,
    UnitDestroyed,
    ResourceChanged,
    // Unit damage, resource depletion, game-state (match start/pause/game-over
    // victory), and UI (menu actions,
    // production ordered). Game-state + UI are dispatched by the menu/match
    // flow; UnitDamaged/ResourceDepleted reserve the unit/resource slots
    // (combat and gather ticks stay dispatcher-free for now).
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
