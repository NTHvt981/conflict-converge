#pragma once

// M1 Goal 5 placeholder: full event dispatcher arrives in Goal 5.
// Forward-declared shape so M3/M4/M5 code can reference events without renames.

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
