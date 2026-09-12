// Unit tests for the M1 Goal 5 EventDispatcher (routing, order, isolation).

#include "test_harness.h"

#include "Event.h"

#include <string>
#include <vector>

namespace
{

struct SpawnedEvent : Event
{
    int teamID = 0;
};

} // namespace

void RunEventTests()
{
    // --- routing + subscription order ---
    EventDispatcher dispatcher;
    std::vector<std::string> calls;
    dispatcher.Subscribe(EventType::UnitSpawned, [&](const Event &) { calls.push_back("first"); });
    dispatcher.Subscribe(EventType::UnitSpawned, [&](const Event &) { calls.push_back("second"); });

    SpawnedEvent spawned;
    spawned.type = EventType::UnitSpawned;
    spawned.teamID = 3;
    dispatcher.Dispatch(spawned);

    CC_CHECK(calls.size() == 2);
    CC_CHECK(calls[0] == "first");
    CC_CHECK(calls[1] == "second");

    // --- handlers receive the concrete payload via downcast ---
    int seenTeam = -1;
    dispatcher.Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
        seenTeam = static_cast<const SpawnedEvent &>(e).teamID;
    });
    SpawnedEvent destroyed;
    destroyed.type = EventType::UnitDestroyed;
    destroyed.teamID = 7;
    dispatcher.Dispatch(destroyed);
    CC_CHECK(seenTeam == 7);

    // --- event types are isolated: UnitSpawned handlers did not re-fire ---
    CC_CHECK(calls.size() == 2);

    // --- dispatch with no listeners is a safe no-op ---
    EventDispatcher empty;
    Event none;
    none.type = EventType::ResourceChanged;
    empty.Dispatch(none);
    CC_CHECK(true);

    // --- Clear drops every subscription ---
    dispatcher.Clear();
    dispatcher.Dispatch(spawned);
    dispatcher.Dispatch(destroyed);
    CC_CHECK(calls.size() == 2);
    CC_CHECK(seenTeam == 7);
}
