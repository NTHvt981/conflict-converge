// Unit tests for the EventDispatcher (routing, order, isolation).

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

    // --- Game-state + UI categories ride the same dispatcher ---
    EventDispatcher flow;
    std::vector<EventType> seen;
    flow.Subscribe(EventType::MatchStarted,
                   [&](const Event &e) { seen.push_back(e.type); });
    flow.Subscribe(EventType::MatchPaused, [&](const Event &e) { seen.push_back(e.type); });
    flow.Subscribe(EventType::GameOver, [&](const Event &e) { seen.push_back(e.type); });
    flow.Subscribe(EventType::Victory, [&](const Event &e) { seen.push_back(e.type); });
    flow.Subscribe(EventType::MenuAction, [&](const Event &e) { seen.push_back(e.type); });
    flow.Subscribe(EventType::ProductionOrdered,
                   [&](const Event &e) { seen.push_back(e.type); });
    Event bare;
    bare.type = EventType::MenuAction;
    flow.Dispatch(bare);
    bare.type = EventType::MatchStarted;
    flow.Dispatch(bare);
    bare.type = EventType::MatchPaused;
    flow.Dispatch(bare);
    bare.type = EventType::ProductionOrdered;
    flow.Dispatch(bare);
    bare.type = EventType::Victory;
    flow.Dispatch(bare);
    CC_CHECK(seen.size() == 5);
    CC_CHECK(seen[0] == EventType::MenuAction);
    CC_CHECK(seen[1] == EventType::MatchStarted);
    CC_CHECK(seen[2] == EventType::MatchPaused);
    CC_CHECK(seen[3] == EventType::ProductionOrdered);
    CC_CHECK(seen[4] == EventType::Victory);
}
