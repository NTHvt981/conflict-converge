#pragma once

#include <cstddef>
#include <vector>

#include "Unit.h"
#include "raylib.h"

class ResourceSystem; // fwd-decl (Production.cpp includes ResourceSystem.h)
class UnitFactory;    // fwd-decl (Production.cpp includes UnitFactory.h)

// Unit production queue. Costs are charged upfront at Enqueue
// (Command & Conquer style), so completion spawns prepaid: pass a UnitFactory
// to Update and finished items roll out at the rally point with full events.

// Placeholder build times (seconds); balance pass tunes these.
float BuildTime(UnitType type);

class ProductionQueue
{
public:
    // Charge CostOf(type) via TrySpend and queue the build. False (nothing
    // queued, nothing spent) when funds are short.
    bool Enqueue(ResourceSystem &resources, UnitType type, bool repeat = false);
    // Drop the head item, refunding its full cost. No-op when empty.
    void CancelTop(ResourceSystem &resources);
    // Repeat-arming for the factory panel's per-type toggle: the next
    // Enqueue of `type` (or an in-panel direct arm) builds with repeat on.
    // Stored on the queue so the stateless immediate-mode panel can read
    // and flip it without Game-side state.
    void SetRepeatArmed(UnitType type, bool repeat);
    bool RepeatArmed(UnitType type) const;
    // Advance the head build; finished units SpawnPrepaid at rallyPos.
    // Returns the spawned entity, or kInvalidEntity when nothing completed
    // this call (QoL: lets callers tag fresh production, e.g. control groups).
    // Repeat items re-charge via resources on each completion (parked when
    // broke), so Update needs the ledger, not just the factory.
    Entity Update(UnitFactory &factory, ResourceSystem &resources, int teamID, Vector2 rallyPos,
                  float dt);

    bool Empty() const;
    std::size_t Size() const;
    float HeadProgress() const; // 0..1, 0 when empty

private:
    struct Item
    {
        UnitType type = UnitType::Infantry;
        float progress = 0.0f;
        float buildTime = 1.0f;
        bool repeat = false; // QoL: re-charge and restart instead of popping
    };
    static constexpr int kTypeCount = static_cast<int>(UnitType::Count);
    std::vector<Item> items_;
    bool repeatArmed_[kTypeCount] = {}; // per-type panel toggle state
};
