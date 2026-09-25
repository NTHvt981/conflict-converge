#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "Subsystem.h"

// Handlers downcast the base Event to the concrete payload.

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

    void Subscribe(EventType type, Handler handler);
    // In subscription order.
    void Dispatch(const Event &event) const;
    void Clear();

private:
    std::unordered_map<EventType, std::vector<Handler>> listeners_;
};
