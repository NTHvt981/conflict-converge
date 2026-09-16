// Unit tests for QoL attack/event pings (raise/floor/expiry/jump) and the
// UnitLifecycleEvent payload carried by factory spawn/destroy dispatch.

#include "test_harness.h"

#include "Event.h"
#include "Pings.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "Unit.h"
#include "UnitFactory.h"

void RunPingTests()
{
    // --- raise + active + latest ---
    {
        Pings pings;
        Vector2 pos = {};
        CC_CHECK(!pings.Latest(pos)); // empty: no jump target
        CC_CHECK(pings.Active().empty());

        pings.RaiseAt({ 100.0f, 200.0f }, PingKind::UnderAttack, 0.0);
        CC_CHECK(pings.Active().size() == 1);
        CC_CHECK(pings.Latest(pos));
        CC_CHECK(pos.x == 100.0f && pos.y == 200.0f);
    }

    // --- same-kind retrigger floor swallows spam, other kinds pass ---
    {
        Pings pings;
        pings.RaiseAt({ 0.0f, 0.0f }, PingKind::UnderAttack, 10.0);
        pings.RaiseAt({ 1.0f, 1.0f }, PingKind::UnderAttack, 11.0); // < 2s: dropped
        CC_CHECK(pings.Active().size() == 1);
        pings.RaiseAt({ 2.0f, 2.0f }, PingKind::UnitLost, 11.0); // other kind: kept
        CC_CHECK(pings.Active().size() == 2);
        pings.RaiseAt({ 3.0f, 3.0f }, PingKind::UnderAttack, 13.0); // floor passed
        CC_CHECK(pings.Active().size() == 3);
    }

    // --- aging expiry + dt<=0 no-op ---
    {
        Pings pings;
        pings.RaiseAt({ 0.0f, 0.0f }, PingKind::UnderAttack, 0.0);
        pings.Update(0.0f);
        CC_CHECK(pings.Active().size() == 1); // no time passed
        pings.Update(-1.0f);
        CC_CHECK(pings.Active().size() == 1); // degenerate dt: untouched
        pings.Update(4.9f);
        CC_CHECK(pings.Active().size() == 1); // just inside lifetime
        pings.Update(0.2f);
        CC_CHECK(pings.Active().empty()); // aged out past 5s
        Vector2 pos = {};
        CC_CHECK(!pings.Latest(pos));
    }

    // --- factory lifecycle events carry position + team ---
    {
        Registry registry;
        ResourceSystem resources;
        resources.AddIron(1000);
        resources.AddOil(500);
        EventDispatcher events;
        Vector2 spawnPos = {};
        int spawnTeam = -1;
        Vector2 deathPos = { -1.0f, -1.0f };
        int deathTeam = -1;
        events.Subscribe(EventType::UnitSpawned, [&](const Event &e) {
            if (const auto *lifecycle = dynamic_cast<const UnitLifecycleEvent *>(&e))
            {
                spawnPos = lifecycle->position;
                spawnTeam = lifecycle->teamID;
            }
        });
        events.Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
            if (const auto *lifecycle = dynamic_cast<const UnitLifecycleEvent *>(&e))
            {
                deathPos = lifecycle->position;
                deathTeam = lifecycle->teamID;
            }
        });
        UnitFactory factory(registry, resources, events);
        const Entity id =
            factory.Spawn(UnitType::Infantry, 1, { 128.0f, 192.0f });
        CC_CHECK(id != kInvalidEntity);
        CC_CHECK(spawnPos.x == 128.0f && spawnPos.y == 192.0f);
        CC_CHECK(spawnTeam == 1);
        factory.DestroyUnit(id);
        CC_CHECK(deathTeam == 1);
        CC_CHECK(deathPos.x == 128.0f && deathPos.y == 192.0f);
    }
}
